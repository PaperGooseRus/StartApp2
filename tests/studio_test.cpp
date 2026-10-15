#include "document.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <chrono>
namespace {
void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
std::string sceneBytes(const pixel::Scene& scene){std::ostringstream out;scene.save(out);return out.str();}
struct Temporary { std::filesystem::path path=std::filesystem::temp_directory_path()/std::filesystem::u8path("студия тест "+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temporary(){std::filesystem::create_directories(path);}~Temporary(){std::error_code e;std::filesystem::remove_all(path,e);} };
}
int main(){try{
    using namespace pixel;using namespace studio;
    AnimationClip clip({{{0,0,.5f,1},100},{{.5f,0,.5f,1},250}},true);
    check(clip.frameAt(0)==0&&clip.frameAt(.099)==0&&clip.frameAt(.1)==1&&clip.frameAt(.349)==1&&clip.frameAt(.35)==0,"animation boundary/loop");
    AnimationClip once(clip.frames(),false);check(once.frameAt(100000)==1,"non looping clip holds last frame");
    check(clip.frameAt(1e200)<2,"large finite animation time");
    bool rejected=false;try{clip.frameAt(std::numeric_limits<double>::quiet_NaN());}catch(const std::invalid_argument&){rejected=true;}check(rejected,"NaN animation time rejected");
    rejected=false;try{AnimationClip bad({{{0,0,1,1},0}});}catch(const std::invalid_argument&){rejected=true;}check(rejected,"zero frame duration rejected");
    Temporary temp;Image image(8,8);image.line(0,0,7,7,{255,0,0,255});check(image.at(3,3).r==255&&image.at(2,3).a==0,"Bresenham diagonal");
    image.floodFill(0,7,{0,255,0,128});check(image.at(0,7).g==255&&image.at(7,0).a==0,"flood fill respects connected regions");
    const auto png=temp.path/std::filesystem::u8path("спрайт.png");image.savePng(png);check(Image::loadPng(png)==image,"PNG RGBA round trip");
    const auto old=image;rejected=false;try{image.line(-1,0,4,4,{1,2,3,4});}catch(const std::invalid_argument&){rejected=true;}check(rejected&&image==old,"invalid stroke leaves image unchanged");
    auto project=Project::example();auto& scene=project.game.scene;const auto gate=project.game.gate;
    auto object=scene.object(gate);object.name="Дверь \"в дом\"";object.position.x=40;scene.replace(gate,object);
    const auto deleted=scene.add(object);scene.erase(deleted);const auto fresh=scene.add(object);check(fresh>deleted&&!scene.object(deleted).active,"deleted object IDs are not reused");
    const auto bytes=sceneBytes(scene);std::istringstream input(bytes);auto restored=Scene::load(input);
    check(sceneBytes(restored)==bytes&&restored.object(gate).name==object.name,"scene round trip preserves IDs, names, animation and components");
    const auto before=sceneBytes(scene);object.visuals[0].bounds.width=-1;rejected=false;try{scene.replace(gate,object);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&sceneBytes(scene)==before,"invalid scene replacement is atomic");
    const auto file=temp.path/std::filesystem::u8path("игра с пробелами.pxproj");studio::save(project,file);auto loaded=studio::load(file);
    check(sceneBytes(loaded.game.scene)==sceneBytes(project.game.scene),"project preserves edited scene");
    check(loaded.art[0].frames==project.art[0].frames&&loaded.art[0].durations==project.art[0].durations,"project preserves art and durations");
    const auto strip=temp.path/"strip.png";project.art[0].atlas().savePng(strip);const auto imported=importPng(strip,4,20);check(imported.frames==project.art[0].frames,"horizontal strip import");
    auto invalid=project;invalid.art[0].durations[0]=0;rejected=false;try{studio::save(invalid,file);}catch(const std::exception&){rejected=true;}
    check(rejected&&studio::load(file).art[0].durations[0]==140,"invalid project does not replace existing file");
    {std::fstream damage(file,std::ios::in|std::ios::out|std::ios::binary);damage.seekp(22);damage.put('!');}
    rejected=false;try{studio::load(file);}catch(const std::exception&){rejected=true;}check(rejected,"project checksum rejects corruption");
    Document document;const auto pixel=document.project.art[0].frames[0].at(0,0);document.beginChange();document.project.art[0].frames[0].at(0,0)={10,20,30,255};document.changed();document.commit();
    check(document.undo()&&document.project.art[0].frames[0].at(0,0).a==pixel.a,"canvas undo");check(document.redo()&&document.project.art[0].frames[0].at(0,0).r==10,"canvas redo");
    const auto frames=document.project.art[0].frames.size();document.duplicateFrame(0,0);check(document.project.art[0].frames.size()==frames+1&&document.project.art[0].frames[1]==document.project.art[0].frames[0],"duplicate frame");
    document.moveFrame(0,0,3);document.removeFrame(0,1);check(document.project.art[0].frames.size()==frames,"move and remove frames");
    const auto originalX=document.project.game.x; const auto originalWater=document.project.game.world.mass(Material::Water);
    document.start(false);for(int i=0;i<20;++i)document.step(1,false);check(document.runtime->x>originalX,"editor preview runs engine");
    document.runtime->world.set(1,1,Material::Water);document.stop();check(document.project.game.x==originalX&&document.project.game.world.mass(Material::Water)==originalWater,"preview does not modify source scene");
    auto door=document.place("door",{23,50});document.project.game.x=20;check(document.project.game.interactionTarget()->object==door,"new door target independent of built-in IDs");
    check(document.project.game.interact()==Sandbox::Interaction::Opened&&!document.project.game.scene.object(door).collider->enabled,"placed door opens using game behavior");
    const auto artCount=document.project.art.size();document.addArt(24,24);check(document.project.art.size()==artCount+1,"new sprite");document.undo();check(document.project.art.size()==artCount,"new sprite undo");
    std::cout<<"Studio checks passed: animation timing, PNG, brush/fill, stable scene IDs, scene/project round trip, corruption, atomic replacement, history, timeline, isolated preview, placed doors\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
