// Host-side test of the REAL firmware pulse + calibration code (pulse.h, settings.h).
#include "Arduino.h"
#include "soc/ledc_struct.h"
uint32_t g_div=0, g_duty=0; bool g_stopped=true; int g_updates=0; ledc_dev_t LEDC;
#include "../gpsspeed_v4/pulse.h"
static int fails=0;
#define CHECK(c,...) do{ if(!(c)){ printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } }while(0)
// what the real hardware would output for the divider the code programmed
static double hwHz(){ return 40e6*256.0/((double)g_div*16384.0); }

int main(){
  pulseInit();
  // 1) frequency accuracy across the whole speed range
  double worst=0, worstAt=0;
  for(double mph=1.0; mph<=60.0; mph+=0.1){
    pulseSetMph((float)mph);
    double want=mph*cfg.hzPerMph;
    if(mph < cfg.minMph){ CHECK(!pulseRunning,"should idle at %.1f",mph); continue; }
    double err=fabs(hwHz()-want)/want;
    if(err>worst){worst=err;worstAt=mph;}
    CHECK(pulseRunning && !g_stopped && g_duty==8192, "not running 50%% at %.1f", mph);
    CHECK(fabs(pulseHz-hwHz())<1e-3, "reported Hz mismatch at %.1f", mph);
  }
  printf("frequency: worst error %.4f%% (at %.1f MPH) over 1-60 MPH\n", worst*100, worstAt);
  CHECK(worst < 0.0005, "frequency error too large");
  // 2) idle behaviour
  pulseSetMph(0.3f); CHECK(!pulseRunning && g_stopped, "0.3 MPH should idle (below min)");
  pulseSetMph(0.0f); CHECK(!pulseRunning, "0 should idle");
  pulseSetMph(5.0f); CHECK(pulseRunning, "5 MPH (no-fix hold) should pulse: %.3f Hz", pulseHz);
  printf("no-fix hold: 5.0 MPH -> %.3f Hz\n", pulseHz);
  // 3) calibration: fake dash that reads 3%% high at 10 MPH drifting to 0.5%% low at 30
  auto dash=[&](double hz){ double m=hz/ DEF_HZ_PER_MPH; return m*(1.03-0.00175*(m-10)); };
  const float pts[]={10,15,20,22,24,26,30};
  printf("before cal: "); for(float p:pts){ pulseSetMph(p); printf("%.0f->%.2f ",p,dash(hwHz())); } printf("\n");
  for(float p:pts){ pulseSetMph(p); CHECK(calAddPoint(p,(float)dash(hwHz())),"cal point %.0f rejected",p); }
  printf("after cal:  "); double calworst=0;
  for(float p:pts){ pulseSetMph(p); double d=dash(hwHz()); calworst=std::max(calworst,fabs(d-p)); printf("%.0f->%.2f ",p,d); } printf("\n");
  double mid=0; for(double m=10;m<=30;m+=0.5){ pulseSetMph((float)m); mid=std::max(mid,fabs(dash(hwHz())-m)); }
  printf("calibrated dash error: %.3f MPH at points, %.3f MPH worst anywhere 10-30\n", calworst, mid);
  CHECK(calworst<0.02,"calibration points not exact"); CHECK(mid<0.1,"interpolated error >0.1 MPH");
  // 4) second pass refines rather than compounds
  for(float p:pts){ pulseSetMph(p); calAddPoint(p,(float)dash(hwHz())); }
  double again=0; for(float p:pts){ pulseSetMph(p); again=std::max(again,fabs(dash(hwHz())-p)); }
  CHECK(again<0.02,"re-calibration drifted (%.3f)",again);
  CHECK(!calAddPoint(20,45), "should refuse a 2x-off reading");
  CHECK(!calAddPoint(1.0f,1.0f), "should refuse points under 2 MPH");
  printf("%s (%d failures)\n", fails?"TESTS FAILED":"ALL TESTS PASSED", fails);
  return fails?1:0;
}
