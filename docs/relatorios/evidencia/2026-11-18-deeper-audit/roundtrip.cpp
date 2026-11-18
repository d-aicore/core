#include "history.hpp"
#include "edit.hpp"
#include "randomize.hpp"
#include "samples.hpp"
#include <iostream>
using namespace core::doc;
void show(const char* name,const History& h,const Document& base){std::cout<<name<<": equal="<<sameContent(base,h.current())<<" modified="<<h.modified()<<" undo="<<h.canUndo()<<" redo="<<h.canRedo()<<"\n";}
int main(){
 auto d=newEmptyDocument();
 {History h(d);auto n=d;n.bpm=150;h.commit(n,"Tempo");h.commit(d,"Tempo");show("two scalar changes",h,d);}
 {History h(d);h.beginGesture("Tempo");auto n=d;n.bpm=150;h.updateGesture(n);h.updateGesture(d);h.endGesture();show("one scalar drag",h,d);}
 {History h(d);auto n=d;n.bpm=150;h.commit(n,"Tempo");n.bpm=160;h.commit(n,"Tempo");h.commit(d,"Tempo");show("three scalar changes",h,d);h.undo();std::cout<<"undo after roundtrip BPM="<<h.current().bpm<<"\n";}
 auto id=addTrack(d,"bass");Note note;note.id="n1";note.length=3500;d.tracks[0].patterns[0].notes.push_back(note);
 {History h(d);auto n=d;setTempo(n,300,{});h.commit(n,"Tempo");setTempo(n,d.bpm,{});h.commit(n,"Tempo");show("tempo 145 to 300 to 145",h,d);std::cout<<"pattern counts before="<<d.tracks[0].patterns.size()<<" after="<<n.tracks[0].patterns.size()<<" activeLength="<<n.tracks[0].findPattern(n.tracks[0].clips[0].patternId)->notes[0].length<<" active="<<n.tracks[0].clips[0].patternId<<"\n";}
 {History h(d);h.beginGesture("Tempo");auto n=d;setTempo(n,300,{});h.updateGesture(n);n=d;setTempo(n,d.bpm,{});h.updateGesture(n);h.endGesture();show("tempo same drag baseline reset",h,d);}
 {auto song=newEmptyDocument();auto id=addTrack(song,"kick"); SampleIndex index; LibrarySample sample; sample.file="/tmp/synthetic-kick.wav";sample.name="Synthetic kick";sample.info.role="kick"; setTrackSample(song,id,index,sample); auto p=toProjection(song); std::cout<<"assign sample: documentKind="<<song.tracks[0].instrument.kind<<" engineInstrument="<<p["tracks"][0]["instrument"]<<"\n";}
 for(const auto& c:builtinInstruments()){auto song=newEmptyDocument();addTrack(song,c.name);std::cout<<"menu instrument "<<c.name<<" -> "<<song.tracks[0].instrument.kind<<"\n";}
}
