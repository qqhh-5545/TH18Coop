using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace TH18LocalCoop {
    internal sealed class InputFrame {
        internal uint Sequence,P1,P2,P3,Stage,Phase,Owner,Flags,Checksum;
        internal bool ThreeSeatFormat;
        internal uint[] Words() {return ThreeSeatFormat?new[]{Sequence,P1,P2,Stage,Phase,Owner,Flags,P3}:new[]{Sequence,P1,P2,Stage,Phase,Owner,Flags};}
        internal uint Hash() {
            uint hash=2166136261;
            foreach(uint word in Words())for(int shift=0;shift<32;shift+=8)hash=unchecked((hash^(byte)(word>>shift))*16777619);
            return hash;
        }
        internal string Encode() {return String.Join("|",Words())+"|"+Checksum;}
        internal static InputFrame Decode(string value) {
            string[] fields=value.Split('|');var words=new uint[fields.Length];
            if(fields.Length!=8 && fields.Length!=9)throw new IOException("INPUT_FORMAT");
            for(int i=0;i<words.Length;++i)if(!UInt32.TryParse(fields[i],out words[i]))throw new IOException("INPUT_FORMAT");
            var frame=FromWords(words);frame.Validate();return frame;
        }
        internal static InputFrame FromWords(uint[] w) {
            return new InputFrame {Sequence=w[0],P1=w[1],P2=w[2],Stage=w[3],Phase=w[4],Owner=w[5],Flags=w[6],P3=w.Length==9?w[7]:0,Checksum=w[w.Length-1],ThreeSeatFormat=w.Length==9};
        }
        internal void Validate() {
            if((P2&~0xcfbu)!=0 || (P3&~0xcfbu)!=0 || Owner>3 || Checksum!=Hash())throw new IOException("INPUT_CHECKSUM");
        }
    }
    internal sealed class InputTape {
        internal string Build;
        internal readonly List<InputFrame> Frames=new List<InputFrame>();
        internal static InputTape Read(string path) {
            using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite))using(var reader=new BinaryReader(file)) {
                long length=file.Length;
                if(length<32 || length>16*1024*1024)throw new IOException("输入记录长度不完整或超过上限。");
                string magic=Encoding.ASCII.GetString(reader.ReadBytes(8));uint format=reader.ReadUInt32(),recordSize=reader.ReadUInt32();
                if(magic!="TH18INP\0" || !((format==1 && recordSize==32)||(format==2 && recordSize==36)) || (length-32)%recordSize!=0)
                    throw new IOException("输入记录格式不支持。");
                byte[] build=reader.ReadBytes(16);int end=Array.IndexOf(build,(byte)0);
                if(end<1)throw new IOException("输入记录版本无效。");
                for(int i=0;i<end;++i)if(build[i]<32 || build[i]>126)throw new IOException("输入记录版本无效。");
                var tape=new InputTape {Build=Encoding.ASCII.GetString(build,0,end)};
                for(uint index=0;file.Position<length;++index) {
                    var words=new uint[recordSize/4];for(int i=0;i<words.Length;++i)words[i]=reader.ReadUInt32();
                    InputFrame frame=InputFrame.FromWords(words);frame.Validate();
                    if(frame.Sequence!=index)throw new IOException("输入记录序号缺失或重复："+index);
                    tape.Frames.Add(frame);
                }
                return tape;
            }
        }
        internal static InputTape Fixture() {
            var tape=new InputTape {Build=NetIdentity.Version};
            for(uint i=0;i<240;++i) {
                var frame=new InputFrame {Sequence=i,P1=(i*31)&0xfffff,P2=(i*17)&0xcfb,Stage=i/80,Phase=i<80?6:uint.MaxValue,Owner=i<40?1u:i<80?2u:0u};
                frame.Checksum=frame.Hash();tape.Frames.Add(frame);
            }
            return tape;
        }
        internal string Summary() {
            int p1=0,p2=0,p3=0,owner1=0,owner2=0,owner3=0;uint previous1=0,previous2=0,previous3=0;
            foreach(var frame in Frames) {
                if(frame.P1!=previous1)++p1;if(frame.P2!=previous2)++p2;
                if(frame.P3!=previous3)++p3;previous3=frame.P3;
                previous1=frame.P1;previous2=frame.P2;
                if(frame.Owner==1)++owner1;if(frame.Owner==2)++owner2;if(frame.Owner==3)++owner3;
            }
            return "输入记录检查通过：版本 "+Build+"，输入轮询 "+Frames.Count+" 次；P1/P2/P3 按键变化 "+p1+"/"+p2+"/"+p3+
                " 次；选人轮询 P1/P2/P3="+owner1+"/"+owner2+"/"+owner3+"。序号和逐条校验完整；这不是可播放的游戏 replay。";
        }
    }
}
