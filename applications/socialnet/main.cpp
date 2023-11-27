#include "video.hpp"
#include <vector>
#include "constants.h"
#include <Grappa.hpp>
#include <Delegate.hpp>
#include "frame.hpp"
#include <unordered_set>
#include <random>
#include <AsyncDelegate.hpp>
#include <GlobalAllocator.hpp>
#include <ParallelLoop.hpp>
#include <Collective.hpp>

using namespace Grappa;
GlobalCompletionEvent gce;
// GlobalCompletionEvent unique_id_gce;
// GlobalCompletionEvent preview_gce;
// GlobalCompletionEvent text_gce;
// GlobalCompletionEvent store_gce;
GlobalCompletionEvent mygce;

std::vector<Video> videos;
std::unordered_set<size_t> unique_ids;

size_t generate_unique_id(){
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> range(0, 1000000000000000000ULL);

    size_t id;
    do {
        id = range(rng);
    } while (unique_ids.find(id) != unique_ids.end());

    unique_ids.insert(id);
    return id;
}

std::tuple<uint32_t, uint32_t, size_t> process_video(int width, int height, size_t video_id){
    std::cout << "Start Processing video " << video_id << " on core " << mycore() << std::endl;
    uint32_t num_video_server = cores() < 32 ? cores() : 32;
    Core video_server_core = video_id % num_video_server;
    uint32_t video_index_on_server = video_id % VIDEOS_PER_CORE;

 
    for(int i = 0; i < MAX_FRAMES; i++){
        // std::cout << "Get frameaddrs of video " << video_id << "frame " << i << " on core " << mycore() << std::endl;
        subFrameGlobalAddrs subframeaddrs = delegate::call(video_server_core, [=]{
            Video v = videos[video_index_on_server];
            subFrameGlobalAddrs addrs = v.getSubFrameAddrs(i);
            return addrs;
        });
        cv::Mat frame = subFrametoMat(subframeaddrs);
        process_frame(frame);

        // write the processed frame to video server by subframe
        for(int j = 0; j < NUM_SUB_FRAMES; j++){
            std::shared_ptr<SubFrame> this_sf = std::make_shared<SubFrame>();
            memcpy(this_sf->data, frame.data + i * SUB_FRAME_SIZE, SUB_FRAME_SIZE);
            GlobalAddress<SubFrame> this_subframe_addr = make_global(this_sf.get());

            GlobalAddress<SubFrame> sf_addr = delegate::call(video_server_core, [=]{
                std::shared_ptr<SubFrame> sf = std::make_shared<SubFrame>();
                GlobalAddress<SubFrame> sf_addr = make_global(sf.get());
                return sf_addr;
            });
            Grappa::memcpy(sf_addr, this_subframe_addr, 1);
            global_free(sf_addr);
            global_free(this_subframe_addr);
        }
        // std::cout << "Process frame " << i << " of video " << video_id << " on core " << mycore() << " done" << std::endl;

        std::cout << "Process frame of video " << video_id << " on core " << mycore() << " done" << std::endl;
    }

    size_t preview_id = delegate::call(video_server_core, [=]{
        // generate random id as stored video id
        std::random_device rd;  // Non-deterministic random number generator
        std::mt19937 rng(rd()); // Mersenne Twister RNG

        // Uniform distribution in range [0, 1000000000000000000)
        std::uniform_int_distribution<size_t> range(0, 1000000000000000000ULL);

        // Generate a random number within the specified range
        size_t id = range(rng);
        return id;
    });

    return std::make_tuple(width, height, preview_id);
}

void compose_post(size_t req_id, const std::string& username, size_t user_id, std::string text, size_t media_id, const std::string& media_type, uint8_t post_type) {
    double task_start = walltime();
    std::cout << "Task " << req_id << " on core " << mycore() << " start" << std::endl;
    
    // unique id
    Core unique_id_core = req_id % 16; // unique id run on the first 16 cores
    size_t unique_id = delegate::call(unique_id_core, [=]{
        size_t uid = generate_unique_id();
        return uid;
    });

    size_t video_id = unique_id;

    uint32_t _width = FRAME_WIDTH;
    uint32_t _height = FRAME_HEIGHT;

    // read preview_width, preview_height, preview_id from process_video
    uint32_t preview_width, preview_height;
    size_t preview_id;
    std::tie(preview_width, preview_height, preview_id) = process_video(_width, _height, video_id);

    std::cout << "Task " << req_id << " on core " << mycore() << " preview id " << preview_id << std::endl;
    std::cout << "Task " << req_id << " on core " << mycore() << " takes " << walltime() - task_start << std::endl;
}




int main(int argc, char *argv[])
{
    init(&argc, &argv);

    run([]
        {
            double video_start = walltime();
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
                    assert(videos.size() == VIDEOS_PER_CORE);
                    std::cout << "Core " << mycore() << ": videos duplicated." << std::endl;
                }
            });
            // gce.wait();
            std::cout << "Video decoding takes " << walltime() - video_start << std::endl;
            // run socialnet benchmark on root task (core 0)

            // gam version is a async function, may change later
            double start = walltime();
            double t_st = start;
            double t_ed_mid = start;


            for(int i = 0; i < 4; ++i) {
                finish([=]{
                    forall<TaskMode::Unbound, SyncMode::Async>(i*50, 50, [](int64_t id){
                        compose_post(id, "username", id, "0 1467810672 Mon Apr 06 22:19:49 PDT 2009 NO_QUERY scotthamilton is upset that he can't update his Facebook by texting it... and might cry as a result  School today also. Blah!",  id, "video",  static_cast<uint8_t>(0));
                    });
                });

                if(i == 1){
                    t_ed_mid = walltime();
                }
                std::cout << "Iteration " << i << " takes " << walltime() - t_st << std::endl;
                t_st = walltime();
            }
            std::cout << "Elapsed time: " << walltime() - start << std::endl;
            std::cout << "Half time: " << walltime() - t_ed_mid << std::endl;

            
            // total videos is (the minimum of 32 and cores()) * VIDEOS_PER_CORE
            // int32_t num_video_server = cores() < 32 ? cores() : 32;
            // int32_t total_videos = num_video_server * VIDEOS_PER_CORE;
            // std::cout << "Total videos: " << total_videos << std::endl;

            // // read one video back
            // uint32_t video_index = 2888;
            // // uint32_t video_index = 7;
            // uint32_t server_index = video_index / VIDEOS_PER_CORE;
            // uint32_t video_index_on_server = video_index % VIDEOS_PER_CORE;

            // std::cout << "Read video " << video_index << " from server " << server_index << " video index on server " << video_index_on_server << std::endl;

            // FrameGlobalAddrs frameaddrs = delegate::call(server_index, [=]{
            //     Video v = videos[video_index_on_server];
            //     FrameGlobalAddrs addrs = v.getFrameAddrs();
            //     return addrs;
            // });

            // for(int i = 0; i < MAX_FRAMES; i++){
            //     cv::Mat frame = subFrametoMat(frameaddrs.data[i]);
            //     std::cout << "Frame " << i << " size " << frame.total() * frame.elemSize() << std::endl;
            //     process_frame(frame);
            //     std::string filename = "socialnet-images/frame" + std::to_string(i) + "_video_" + std::to_string(video_index) + "_processed.png";  
            //     if (!cv::imwrite(filename, frame))
            //     {
            //         std::cerr << "Failed to write image" << std::endl;
            //         return 1;
            //     }
            // }
            // for(int i = 0; i < MAX_FRAMES; i++){
            //     subFrameGlobalAddrs subframeaddrs = delegate::call(server_index, [=]{
            //         Video v = videos[video_index_on_server];
            //         subFrameGlobalAddrs addrs = v.getSubFrameAddrs(i);
            //         return addrs;
            //     });
            //     cv::Mat frame = subFrametoMat(subframeaddrs);
            //     std::cout << "Frame " << i << " size " << frame.total() * frame.elemSize() << std::endl;
            //     std::string filename = "socialnet-images/frame" + std::to_string(i) + "_video_" + std::to_string(video_index) + ".png";  
            //     if (!cv::imwrite(filename, frame))
            //     {
            //         std::cerr << "Failed to write image" << std::endl;
            //         return 1;
            //     }
            // }

        
        std::cout << "***** test video done *****" << std::endl; });

    finalize();
    return 0;
}
