#include "edit.hpp"
#include "groove.hpp"
#include "midi.hpp"
#include "pattern_rack.hpp"
#include "randomize.hpp"
#include "history.hpp"
#include <iostream>
namespace d=core::doc;
int main(){
 auto doc=d::newEmptyDocument();auto tid=d::addTrack(doc,"bass");auto& p=doc.tracks[0].patterns[0];d::Note n;n.id="n1";n.length=960;n.velocity=.5;n.hasRoll=true;n.roll.count=4;n.roll.velocityFrom=.3;n.roll.velocityTo=.7;p.notes={n};auto before=d::exportMidi(doc);auto changed=d::scaleVelocities(p,{},1.15);std::cout<<"roll louder changed="<<changed<<" displayedVelocity="<<p.notes[0].velocity<<" exportedMidiUnchanged="<<(before==d::exportMidi(doc))<<"\n";p.notes[0].hasRoll=false;p.notes[0].roll={};before=d::exportMidi(doc);d::scaleVelocities(p,{},1.15);std::cout<<"ordinary louder midiChanged="<<(before!=d::exportMidi(doc))<<"\n";
 auto song=d::newTemplateDocument();song.extra["template"]={{"style","Dark psycore"}};d::addSection(song,"Groove",0,song.lengthTicks);d::ensureRackSlots(song,0);auto sid=d::addRackSlot(song,"Unarranged idea",4);auto& idea=d::rackPatternFor(song,sid,"bass");auto pid=idea.id;idea.notes={n};std::cout<<"before randomize ideaExists="<<(song.findTrack("bass")->findPattern(pid)!=nullptr)<<" rackRefs="<<d::findRackSlot(d::rackSlots(song),sid)->tracks.size();d::History history(song);d::RandomizeOptions o;o.axes={"tempo"};auto lines=d::randomizeStyle(song,o);auto slots=d::rackSlots(song);std::cout<<" afterExists="<<(song.findTrack("bass")->findPattern(pid)!=nullptr)<<" rackRefs="<<d::findRackSlot(slots,sid)->tracks.size()<<" changes="<<lines.size()<<"\n";d::parseDocument(d::toJson(song));history.commit(song,"Randomize tempo");history.undo();std::cout<<"undo ideaRestored="<<(history.current().findTrack("bass")->findPattern(pid)!=nullptr)<<"\n";std::cout<<"reopening slot notes="<<d::rackPatternFor(song,sid,"bass").notes.size()<<"\n";
}
