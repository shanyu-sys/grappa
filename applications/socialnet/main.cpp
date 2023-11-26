#include "video.hpp"
#include <vector>
#include "constants.h"
#include <Grappa.hpp>
#include <Delegate.hpp>
#include "frame.hpp"

using namespace Grappa;
GlobalCompletionEvent gce;

int test_single_read_write(){
    std::cout << "***** test video decode *****" << std::endl;
    Video v = decode();
    std::cout << "video size: " << sizeof(v) << std::endl;
    std::cout << "video width: " << v.width << std::endl;
    std::cout << "video height: " << v.height << std::endl;
    std::cout << "video number of frames: " << v.frames.size() << std::endl;
    assert(v.frames.size() == MAX_FRAMES);

    for (int i = 0; i < MAX_FRAMES; i++)
    {
        subFrameGlobalAddrs subframeaddrs = v.getSubFrameAddrs(i);
        cv::Mat frame = subFrametoMat(subframeaddrs);


        // print addresses
        std::cout << "Output Frame " << i << " subframe 0 globaladdress " << subframeaddrs.data[0] << std::endl;

        std::string filename = "socialnet-images/frame" + std::to_string(i) + ".png";
        if (!cv::imwrite(filename, frame))
        {
            std::cerr << "Failed to write image" << std::endl;
            return 1;
        }
    }

    std::cout << "***** test video deep copy *****" << std::endl;
    Video v2 = v;
    std::cout << "video size: " << sizeof(v2) << std::endl;
    std::cout << "video width: " << v2.width << std::endl;
    std::cout << "video height: " << v2.height << std::endl;
    for (int i = 0; i < MAX_FRAMES; i++)
    {
        auto subframeaddrs = v2.getSubFrameAddrs(i);
        cv::Mat frame = subFrametoMat(subframeaddrs);

        // print addresses
        if(i==3){
            std::cout << "Output Frame " << i << " subframe 0 globaladdress " << subframeaddrs.data[0] << std::endl;
        }

        std::string filename = "socialnet-images/frame-cpy" + std::to_string(i) + ".png";
        if (!cv::imwrite(filename, frame))
        {
            std::cerr << "Failed to write image" << std::endl;
            return 1;
        }
    }

    std::cout << "***** test read video from other cores *****" << std::endl;

    GlobalAddress<Video> v_addr = make_global(&v);

    on_all_cores([=]{
        if(mycore() != 0){
            std::cout << "***** test read video on core " << mycore() << " locale " << mylocale() <<" *****" << std::endl;
            subFrameGlobalAddrs subframeaddrs = delegate::call(v_addr, [](Video *p){
                subFrameGlobalAddrs addrs = p->getSubFrameAddrs(3);
                return addrs;
            });
            cv::Mat frame = subFrametoMat(subframeaddrs);
            
            std::cout << "core " << mycore() << " subframe 0 address " << subframeaddrs.data[0] << std::endl;
            std::cout << "core " << mycore() << " frame 3 size " << frame.total() * frame.elemSize() << std::endl;

            std::string filename = "socialnet-images/frame3_core" + std::to_string(mycore()) + ".png";
            if (!cv::imwrite(filename, frame))
            {
                std::cerr << "Failed to write image" << std::endl;
                return 1;
            }
        }
    });
}


std::vector<Video> videos;
int main(int argc, char *argv[])
{
    init(&argc, &argv);

    run([]
        {
            // video server: at most 32 cores (2 nodes)
            on_all_cores([]{
                if(mycore() < 32){
                    std::cout << "core " << mycore() << " locale " << mylocale() << std::endl;
                    Video v = decode();
                    videos.emplace_back(v);

                    std::cout << "Core " << mycore() << ": video decoded." << std::endl;

                    // deep copy videos for VIDEOS_PER_CORE -1 times
                    for(int i = 0; i < VIDEOS_PER_CORE - 1; i++){
                        Video v2 = Video(v);
                        videos.emplace_back(v2);
                    }
                    std::cout << "Core " << mycore() << ": videos duplicated." << std::endl;

                    // verify the copy
                    for(int i = 0; i < VIDEOS_PER_CORE; i++){
                        Video * v_ptr = &videos[i];
                        for(int j = 0; j < MAX_FRAMES; j++){
                            subFrameGlobalAddrs subframeaddrs = v_ptr->getSubFrameAddrs(j);
                            cv::Mat frame = subFrametoMat(subframeaddrs);
                            if(j == 3 && (i == 0 || i == VIDEOS_PER_CORE - 1)){
                                std::cout << "Core " << mycore() << ": video " << i << " frame " << j << " global address " << subframeaddrs.data[0] << std::endl;
                            }
                        }
                    }

                }
            });
            gce.wait();
            // total videos is (the minimum of 32 and cores()) * VIDEOS_PER_CORE
            int32_t num_video_server = cores() < 32 ? cores() : 32;
            int32_t total_videos = num_video_server * VIDEOS_PER_CORE;
            std::cout << "Total videos: " << total_videos << std::endl;

            // read one video back
            uint32_t video_index = 1080;
            // uint32_t video_index = 7;
            uint32_t server_index = video_index / VIDEOS_PER_CORE;
            uint32_t video_index_on_server = video_index % VIDEOS_PER_CORE;

            std::cout << "Read video " << video_index << " from server " << server_index << " video index on server " << video_index_on_server << std::endl;

            for(int i = 0; i < MAX_FRAMES; i++){
                subFrameGlobalAddrs subframeaddrs = delegate::call(server_index, [=]{
                    Video v = videos[video_index_on_server];
                    subFrameGlobalAddrs addrs = v.getSubFrameAddrs(i);
                    return addrs;
                });
                cv::Mat frame = subFrametoMat(subframeaddrs);
                std::cout << "Frame " << i << " size " << frame.total() * frame.elemSize() << std::endl;
                std::string filename = "socialnet-images/frame" + std::to_string(i) + "_video_" + std::to_string(video_index) + ".png";  
                if (!cv::imwrite(filename, frame))
                {
                    std::cerr << "Failed to write image" << std::endl;
                    return 1;
                }
            }

        std::cout << "***** test video done *****" << std::endl; });

    finalize();
    return 0;
}
