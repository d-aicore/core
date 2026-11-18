#include "edit.hpp"
#include "midi.hpp"
#include "core/project.hpp"
#include "core/tempo_map.hpp"
#include <iostream>
#include <cmath>
namespace d = core::doc;
constexpr int64_t bar = 4*d::kPpq;
double engineTime(const d::Document& doc,int64_t tick){auto p=core::fromJson(d::toProjection(doc,{}));core::TempoMap map(p.bpm,p.tempoMap,48000);return map.sampleAt(tick)/48000;}
int main(){
 {auto doc=d::newEmptyDocument();doc.bpm=120;auto a=d::addSection(doc,"A",0,4*bar);d::addSection(doc,"B",4*bar,4*bar);d::setTempoMap(doc,{{bar,200},{3*bar,160}});auto original=doc;d::duplicateSection(doc,a);std::cout<<"duplicate: originalStart="<<d::bpmAt(original,0)<<" copyStart="<<d::bpmAt(doc,4*bar)<<" originalLater="<<d::bpmAt(original,bar)<<" copyLater="<<d::bpmAt(doc,5*bar)<<" followingBefore="<<d::bpmAt(original,4*bar)<<" followingAfter="<<d::bpmAt(doc,8*bar)<<"\n";d::parseDocument(d::toJson(doc));}
 {auto doc=d::newEmptyDocument();doc.bpm=120;auto a=d::addSection(doc,"A",0,4*bar);d::addSection(doc,"B",4*bar,4*bar);d::setTempoMap(doc,{{4*bar,240}});auto map=d::toJson(doc)["transport"]["tempoMap"];d::swapSection(doc,a,true);std::cout<<"swap: firstName="<<doc.sections[0].name<<" firstBpm="<<d::bpmAt(doc,0)<<" secondName="<<doc.sections[1].name<<" secondBpm="<<d::bpmAt(doc,4*bar)<<" mapUnchanged="<<(map==d::toJson(doc)["transport"]["tempoMap"])<<"\n";d::parseDocument(d::toJson(doc));}
 {auto doc=d::newEmptyDocument();doc.bpm=120;d::setTempoMap(doc,{{4*bar,240,d::json::object(),true}});auto old=doc;double before=d::bpmAt(doc,bar);d::insertTime(doc,2*bar,2*bar);std::cout<<"insert ramp: beforeEditBpmAtBar1="<<before<<" afterEdit="<<d::bpmAt(doc,bar)<<" beforeSeconds="<<engineTime(old,bar)<<" afterSeconds="<<engineTime(doc,bar)<<" originalAt3="<<d::bpmAt(old,3*bar)<<" movedAt5="<<d::bpmAt(doc,5*bar)<<"\n";d::parseDocument(d::toJson(doc));d::deleteTime(doc,2*bar,2*bar);std::cout<<"insert-delete roundtrip: same="<<(d::toJson(doc)==d::toJson(old))<<" bpmAtBar1="<<d::bpmAt(doc,bar)<<"\n";d::parseDocument(d::toJson(doc));}
 {auto doc=d::newEmptyDocument();doc.bpm=120;d::setTempoMap(doc,{{4*bar,240,d::json::object(),true}});auto old=doc;double before=d::bpmAt(doc,bar);d::setSongLength(doc,2*bar);std::cout<<"shorten ramp: beforeEditBpmAtBar1="<<before<<" afterEdit="<<d::bpmAt(doc,bar)<<" beforeSeconds="<<engineTime(old,bar)<<" afterSeconds="<<engineTime(doc,bar)<<" remainingPoints="<<doc.tempoMap.size()<<"\n";d::parseDocument(d::toJson(doc));d::setSongLength(doc,old.lengthTicks);std::cout<<"length roundtrip: same="<<(d::toJson(doc)==d::toJson(old))<<" bpmAtBar1="<<d::bpmAt(doc,bar)<<"\n";d::parseDocument(d::toJson(doc));}
 {auto doc=d::newEmptyDocument();doc.bpm=120;d::setTempoMap(doc,{{4*bar,240}});double before=d::bpmAt(doc,bar);d::insertTime(doc,2*bar,2*bar);std::cout<<"step insertion control: before="<<before<<" after="<<d::bpmAt(doc,bar)<<"\n";}
}
