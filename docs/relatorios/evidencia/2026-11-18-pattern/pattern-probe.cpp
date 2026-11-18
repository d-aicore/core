#include "edit.hpp"
#include "pattern_rack.hpp"
#include <iostream>
namespace d=core::doc;
int main(){
 auto doc=d::newEmptyDocument();auto id=d::addTrack(doc,"bass");doc.tracks[0].name="Bass";d::ensureRackSlots(doc,0);auto slot=d::rackSlots(doc)[0].id;d::Note n;n.id="n1";n.tick=240;n.length=120;doc.tracks[0].patterns[0].notes={n};d::setRackSwing(doc,slot,.5);std::cout<<"swing initial tick="<<doc.tracks[0].patterns[0].notes[0].tick<<"\n";
 d::PatternTemplate t;t.id="test";t.name="Synthetic";t.bars=1;t.rows.push_back({"bass","Bass",3840,{n}});d::applyRackTemplate(doc,nullptr,t,slot,1);std::cout<<"after template swing="<<d::rackSlots(doc)[0].swing<<" tick="<<doc.tracks[0].patterns[0].notes[0].tick;d::setRackSwing(doc,slot,.75);std::cout<<" after .75 tick="<<doc.tracks[0].patterns[0].notes[0].tick<<"\n";
 d::setRackSwing(doc,slot,0);d::setRackSwing(doc,slot,.75);std::cout<<"zero-reset control tick="<<doc.tracks[0].patterns[0].notes[0].tick<<"\n";
 auto bad=d::toJson(d::newEmptyDocument());bad["patternSlots"]=d::json::array({{{"id","s1"},{"bars","four"}}});auto loaded=d::parseDocument(bad);std::cout<<"malformed metadata parse accepted=1 ";try{d::rackSlots(loaded);std::cout<<"rack read accepted\n";}catch(const std::exception&e){std::cout<<"rack read throws="<<e.what()<<"\n";}
 auto song=d::newEmptyDocument();auto b=d::addTrack(song,"bass");auto l=d::addTrack(song,"lead");song.tracks[0].name="Bass";song.tracks[1].name="Lead";d::ensureRackSlots(song,0);auto s=d::rackSlots(song)[0].id;d::Note low=n;low.pitch=36;d::Note high=n;high.pitch=84;d::PatternTemplate mix;mix.id="mixed";mix.name="Synthetic";mix.bars=1;mix.rows={{"bass","Lead",3840,{low}},{"lead","Bass",3840,{high}}};d::applyRackTemplate(song,nullptr,mix,s,1);std::cout<<"mapping bassPitch="<<d::rackPatternFor(song,s,b).notes[0].pitch<<" leadPitch="<<d::rackPatternFor(song,s,l).notes[0].pitch<<"\n";
 d::parseDocument(d::toJson(doc));d::parseDocument(d::toJson(song));
}
