#include "eagler_core.h"
#include <eagler/netplay/NetplayCore.hpp>
#include <new>
#include <cstring>
struct EaglerAdapter {
    Netplay::RollbackCore core;Netplay::FrameDecision decision;
    uint32_t prepared=Netplay::INVALID_FRAME;unsigned role=0,players=0;
};
static uint16_t encode_keys(uint32_t keys){return uint16_t((keys&0xdfbu)|((keys&0x80000u)?0x8000u:0u));}
static uint32_t decode_keys(uint16_t keys){return uint32_t(keys&0xdfbu)|((keys&0x8000u)?0x80000u:0u);}
static void *create_core(unsigned players,unsigned role,unsigned delay,unsigned window,uint64_t session,bool prediction){
    auto *p=new(std::nothrow) EaglerAdapter;if(!p)return nullptr;
    Netplay::CoreConfig cfg;cfg.playerCount=uint8_t(players);cfg.localPlayer=uint8_t(role);cfg.inputDelay=uint8_t(delay);cfg.maxRollbackFrames=uint8_t(window);cfg.sessionId=session;
    cfg.allowPrediction=prediction;cfg.predictableButtons=0xf9;cfg.directionButtons=0xf0;cfg.maxDirectionPredictionFrames=4;
    if(players>3 || role>=players || delay>16 || window>32 || !p->core.Reset(cfg)){delete p;return nullptr;}p->role=role;p->players=players;return p;
}
extern "C" void *ec_create(unsigned p,unsigned r,unsigned d,unsigned w,uint64_t s){return create_core(p,r,d,w,s,true);}
extern "C" void *ec_create_delay(unsigned p,unsigned r,unsigned d,uint64_t s){if(d<1 || d>12)return nullptr;return create_core(p,r,d,8,s,false);}
extern "C" void ec_destroy(void *h){delete static_cast<EaglerAdapter *>(h);}
extern "C" int ec_local(void *h,uint32_t f,uint32_t keys){if(!h || (keys&~0x80dfbu))return 0;return static_cast<EaglerAdapter *>(h)->core.ScheduleLocalInput(f,encode_keys(keys));}
extern "C" int ec_has_local(void *h,uint32_t f){return h && static_cast<EaglerAdapter *>(h)->core.HasLocalCapture(f);}
extern "C" int ec_prepare(void *h,uint32_t f,uint32_t keys[3],unsigned *mask){if(!h || !keys || !mask)return 0;auto *p=static_cast<EaglerAdapter *>(h);auto d=p->core.PrepareFrame(f);if(!d.canAdvance)return 0;
    p->prepared=f;p->decision=d;for(unsigned i=0;i<3;++i)keys[i]=decode_keys(d.inputs[i].buttons);*mask=d.predictedMask;return 1;}
extern "C" int ec_mark(void *h,uint32_t f){if(!h)return 0;auto *p=static_cast<EaglerAdapter *>(h);if(p->prepared!=f)return 0;bool ok=p->core.MarkSimulated(f,p->decision);p->prepared=Netplay::INVALID_FRAME;return ok;}
extern "C" int ec_rewind(void *h,uint32_t f){if(!h)return 0;auto *p=static_cast<EaglerAdapter *>(h);p->prepared=Netplay::INVALID_FRAME;return p->core.RewindSimulationTo(f);}
extern "C" uint32_t ec_rollback(void *h){return h?static_cast<EaglerAdapter *>(h)->core.RollbackFrame():Netplay::INVALID_FRAME;}
extern "C" uint32_t ec_confirmed(void *h){return h?static_cast<EaglerAdapter *>(h)->core.ConfirmedThroughAllRemotes():Netplay::INVALID_FRAME;}
extern "C" int ec_inputs(void *h,uint32_t f,uint32_t keys[3]){if(!h || !keys)return 0;std::array<Netplay::FrameInput,3> a;auto *p=static_cast<EaglerAdapter *>(h);if(!p->core.ConfirmedInputs(f,&a))return 0;for(unsigned i=0;i<3;++i)keys[i]=decode_keys(a[i].buttons);return 1;}
extern "C" int ec_encode(void *h,unsigned peer,uint32_t latest,uint32_t seq,uint8_t *bytes,size_t capacity,size_t *size){if(!h || !bytes || !size)return 0;auto *p=static_cast<EaglerAdapter *>(h);if(peer>=p->players || peer==p->role)return 0;
    try{auto packet=p->core.BuildInputPacket(uint8_t(peer),latest,seq,0);std::vector<uint8_t> encoded;if(!Netplay::EncodeInputPacket(packet,&encoded) || encoded.size()>capacity)return 0;
    std::memcpy(bytes,encoded.data(),encoded.size());*size=encoded.size();return 1;}catch(const std::bad_alloc &){*size=0;return 0;}}
extern "C" int ec_receive(void *h,unsigned sender,const uint8_t *bytes,size_t size){if(!h || !bytes)return 0;auto *p=static_cast<EaglerAdapter *>(h);Netplay::InputPacket packet;
    if(sender>=p->players || sender==p->role || !Netplay::DecodeInputPacket(bytes,size,&packet) || packet.senderPlayer!=sender)return 0;
    for(unsigned i=0;i<packet.inputCount;++i)if(packet.inputs[i].buttons&~0x8dfbu)return 0;
    return p->core.ApplyInputPacket(packet);}
