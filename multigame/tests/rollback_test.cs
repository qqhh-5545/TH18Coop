using System;
using TH18LocalCoop;
class RollbackTest {
 static void Check(bool value){if(!value)throw new Exception("rollback assertion");}
 static void Fails(Action action){bool failed=false;try{action();}catch(InvalidOperationException){failed=true;}Check(failed);}
 static uint Input(int seat,int frame){return ((frame/3+seat)%2==0?0x10u:0x80u)|((frame+seat)%7==0?2u:1u);}
 static void Main(){
  // 3 independent seats, deterministic simulation, missing/out-of-order remote
  // inputs. Restore world before replay; physical inputs are never resampled.
  var ledger=new RollbackTimeline(3,32,8);var effects=new RollbackEffects<int>();var published=new System.Collections.Generic.List<int>();int[] snapshots=new int[160];int state=17,corrections=0;
  for(int wall=0;wall<125;++wall){
   for(int seat=0;seat<3;++seat){int f=wall-(seat==0?0:seat==1?3:5);if(f>=0 && f<120)ledger.Submit(seat,f,Input(seat,f));}
   if(ledger.RollbackFrame.HasValue){long frame=ledger.RollbackFrame.Value;state=snapshots[(int)frame];effects.DiscardFrom(frame);ledger.Rewind(frame);++corrections;}
   long end=Math.Min(120,wall+1);
   while(ledger.NextFrame<end){var d=ledger.Prepare();snapshots[(int)d.Frame]=state;for(int s=0;s<3;++s)state=unchecked(state*31+(int)d.Input[s]);effects.Enqueue(d.Frame,0,state);ledger.MarkSimulated(d);}
   effects.CommitThrough(ledger.SafeToCommitThrough,published.Add);
   ledger.RetireBefore(Math.Max(0,ledger.ConfirmedThrough-8));
  }
  Check(corrections>20 && ledger.NextFrame==120 && ledger.ConfirmedThrough==119 && !ledger.RollbackFrame.HasValue);
  int expected=17;for(int f=0;f<120;++f)for(int s=0;s<3;++s)expected=unchecked(expected*31+(int)Input(s,f));Check(state==expected);
  Check(published.Count==120 && published[119]==state);Fails(()=>effects.DiscardFrom(119));
  ledger.RetireBefore(120);ledger.Submit(0,120,1);ledger.Submit(1,120,0x80);ledger.Submit(2,120,8);ledger.MarkSimulated(ledger.Prepare());Check(ledger.NextFrame==121);
  Fails(()=>ledger.Submit(1,119,1));Fails(()=>ledger.Submit(0,120,2));
  var limits=new RollbackTimeline(2,8,2);limits.MarkSimulated(limits.Prepare());limits.MarkSimulated(limits.Prepare());Fails(()=>limits.Prepare());Fails(()=>limits.RetireBefore(1));
  limits.Submit(0,1,1);limits.Submit(0,0,0);Check(limits.RollbackFrame==1);Fails(()=>limits.Prepare());limits.Rewind(1);Check(limits.Prepare().Input[0]==1);
  var stale=new RollbackTimeline(2);var decision=stale.Prepare();stale.Submit(1,0,0x80);Fails(()=>stale.MarkSimulated(decision));
  Console.WriteLine("PASS rollback input ledger: three seats, late/out-of-order actuals, rollback+replay matches exact-input world, immutable conflicts, bounded prediction/history, ring reuse, retirement and stale-decision guard");
 }
}
