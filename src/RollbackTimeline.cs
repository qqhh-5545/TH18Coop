using System;
using System.Collections.Generic;
namespace TH18LocalCoop {
    // Engine-independent input ledger. NOT enabled by OnlineSession until the
    // native world and confirmed-output owners have passed restoration tests.
    internal sealed class RollbackTimeline {
        sealed class Slot {
            internal long Frame=-1;
            internal readonly uint[] Actual=new uint[3],Used=new uint[3];
            internal uint ActualMask;internal bool Simulated;
        }
        internal sealed class Decision {
            internal long Frame;internal uint PredictedMask;
            internal readonly uint[] Input=new uint[3];
        }
        readonly int players,predictionLimit;readonly Slot[] slots;
        readonly long[] frontier={-1,-1,-1};readonly uint[] retiredInput=new uint[3];
        long floor,next;long? mismatch;
        internal long NextFrame {get{return next;}}
        internal long? RollbackFrame {get{return mismatch;}}
        internal long ConfirmedThrough {get{long f=frontier[0];for(int s=1;s<players;++s)f=Math.Min(f,frontier[s]);return f;}}
        internal long SafeToCommitThrough {get{return Math.Min(Math.Min(ConfirmedThrough,next-1),mismatch.HasValue?mismatch.Value-1:long.MaxValue);}}
        internal RollbackTimeline(int count,int history=64,int maxPrediction=8) {
            if(count<2 || count>3 || history<4 || maxPrediction<0 || maxPrediction>=history)throw new ArgumentException("ROLLBACK_CONFIG");
            players=count;predictionLimit=maxPrediction;slots=new Slot[history];for(int i=0;i<history;++i)slots[i]=new Slot();
        }
        Slot At(long frame,bool create) {
            if(frame<floor || frame-floor>=slots.Length)throw new InvalidOperationException("ROLLBACK_HISTORY");
            Slot slot=slots[(int)(frame%slots.Length)];
            if(slot.Frame!=frame){if(!create)return null;slot.Frame=frame;slot.ActualMask=0;slot.Simulated=false;Array.Clear(slot.Actual,0,3);Array.Clear(slot.Used,0,3);}
            return slot;
        }
        internal void Submit(int seat,long frame,uint input) {
            if(seat<0 || seat>=players || (input&~0xdfbu)!=0)throw new ArgumentException("ROLLBACK_INPUT");
            Slot slot=At(frame,true);uint bit=1u<<seat;
            if((slot.ActualMask&bit)!=0){if(slot.Actual[seat]!=input)throw new InvalidOperationException("ROLLBACK_CONFLICT");return;}
            slot.Actual[seat]=input;slot.ActualMask|=bit;
            if(slot.Simulated && slot.Used[seat]!=input)mismatch=mismatch.HasValue?Math.Min(mismatch.Value,frame):frame;
            while(frontier[seat]+1<floor+slots.Length){Slot candidate=At(frontier[seat]+1,false);if(candidate==null || (candidate.ActualMask&bit)==0)break;++frontier[seat];}
        }
        internal Decision Prepare() {
            if(mismatch.HasValue)throw new InvalidOperationException("ROLLBACK_REQUIRED");
            if(next>ConfirmedThrough+predictionLimit)throw new InvalidOperationException("ROLLBACK_WAIT");
            Slot slot=At(next,true);var decision=new Decision{Frame=next};
            for(int seat=0;seat<players;++seat) {
                uint bit=1u<<seat;
                if((slot.ActualMask&bit)!=0){decision.Input[seat]=slot.Actual[seat];continue;}
                uint value=retiredInput[seat];
                for(long f=next-1;f>=floor;--f){Slot prior=At(f,false);if(prior!=null && (prior.ActualMask&bit)!=0){value=prior.Actual[seat];break;}}
                // Carry movement/focus/shoot only; no repeated Bomb/card/menu edges.
                decision.Input[seat]=value&0xf9u;decision.PredictedMask|=bit;
            }
            return decision;
        }
        internal void MarkSimulated(Decision decision) {
            if(decision==null || decision.Frame!=next || mismatch.HasValue)throw new InvalidOperationException("ROLLBACK_SEQUENCE");
            // Recompute to reject a mutated or stale decision before committing it.
            Decision expected=Prepare();
            for(int seat=0;seat<players;++seat)if(decision.Input[seat]!=expected.Input[seat])throw new InvalidOperationException("ROLLBACK_STALE_INPUT");
            Slot slot=At(next,true);Array.Copy(decision.Input,slot.Used,3);slot.Simulated=true;++next;
        }
        internal void Rewind(long frame) {
            if(!mismatch.HasValue || frame!=mismatch.Value || frame<floor || frame>=next)throw new InvalidOperationException("ROLLBACK_TARGET");
            for(long f=frame;f<next;++f){Slot slot=At(f,false);if(slot!=null)slot.Simulated=false;}
            next=frame;mismatch=null; // Physical inputs and receive frontiers survive.
        }
        internal void RetireBefore(long frame) {
            if(frame<floor || frame>next || frame>ConfirmedThrough+1 || (mismatch.HasValue && frame>mismatch.Value))throw new InvalidOperationException("ROLLBACK_UNCONFIRMED");
            for(long f=floor;f<frame;++f){Slot slot=At(f,false);if(slot==null || !slot.Simulated)throw new InvalidOperationException("ROLLBACK_UNSIMULATED");for(int s=0;s<players;++s)retiredInput[s]=slot.Actual[s];}
            floor=frame;
        }
    }
    internal sealed class RollbackEffects<T> {
        sealed class Entry {internal long Frame;internal int Id;internal T Value;}
        readonly List<Entry> entries=new List<Entry>();readonly int limit;long committed=-1;bool failed;
        internal RollbackEffects(int maxEntries=4096){if(maxEntries<1)throw new ArgumentException("ROLLBACK_OUTPUT_LIMIT");limit=maxEntries;}
        internal void Enqueue(long frame,int id,T value){
            if(failed || frame<=committed || entries.Count==limit)throw new InvalidOperationException("ROLLBACK_OUTPUT_HISTORY");
            foreach(var e in entries)if(e.Frame==frame && e.Id==id)throw new InvalidOperationException("ROLLBACK_DUPLICATE_OUTPUT");
            entries.Add(new Entry{Frame=frame,Id=id,Value=value});
        }
        internal void DiscardFrom(long frame){if(failed || frame<=committed)throw new InvalidOperationException("ROLLBACK_COMMITTED_OUTPUT");entries.RemoveAll(e=>e.Frame>=frame);}
        internal void CommitThrough(long frame,Action<T> publish){
            if(failed || frame<committed)throw new InvalidOperationException("ROLLBACK_COMMIT_ORDER");
            entries.Sort((a,b)=>a.Frame==b.Frame?a.Id.CompareTo(b.Id):a.Frame.CompareTo(b.Frame));
            while(entries.Count>0 && entries[0].Frame<=frame){Entry e=entries[0];entries.RemoveAt(0);try{publish(e.Value);}catch{failed=true;throw;}}
            committed=frame;
        }
    }
}
