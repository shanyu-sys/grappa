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
#include "text.hpp"
#include <chrono>
#include <unordered_map>
#include "post.hpp"

using namespace Grappa;
GlobalCompletionEvent gce;
// GlobalCompletionEvent unique_id_gce;
// GlobalCompletionEvent preview_gce;
// GlobalCompletionEvent text_gce;
// GlobalCompletionEvent store_gce;
GlobalCompletionEvent mygce;

std::vector<Video> videos;
std::unordered_set<size_t> unique_ids;
std::unordered_map<size_t, Post> post_cache;

size_t generate_unique_id()
{
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<size_t> range(0, 1000000000000000000ULL);

    size_t id;
    do
    {
        id = range(rng);
    } while (unique_ids.find(id) != unique_ids.end());

    unique_ids.insert(id);
    return id;
}

std::tuple<uint32_t, uint32_t, size_t> process_video(int width, int height, size_t video_id)
{
    std::cout << "Start Processing video " << video_id << " on core " << mycore() << std::endl;
    uint32_t num_video_server = cores() < 32 ? cores() : 32;
    Core video_server_core = video_id % num_video_server;
    uint32_t video_index_on_server = video_id % VIDEOS_PER_CORE;

    for (int i = 0; i < MAX_FRAMES; i++)
    {
        // std::cout << "Get frameaddrs of video " << video_id << "frame " << i << " on core " << mycore() << std::endl;
        subFrameGlobalAddrs subframeaddrs = delegate::call(video_server_core, [=]
                                                           {
            // std::cout << "Get frameaddrs of video " << video_id << "frame " << i << " on core " << mycore() << std::endl;
            assert(video_index_on_server < videos.size());
            Video *v = &videos[video_index_on_server];
            subFrameGlobalAddrs addrs = v->getSubFrameAddrs(i);
            // std::cout << "Get frameaddrs of video " << video_id << "frame " << i << " on core " << mycore() << " done" << std::endl;
            return addrs; });
        cv::Mat frame = subFrametoMat(subframeaddrs);
        process_frame(frame);

        // write the processed frame to video server by subframe
        for (int j = 0; j < NUM_SUB_FRAMES; j++)
        {
            std::shared_ptr<SubFrame> this_sf = std::make_shared<SubFrame>();
            memcpy(this_sf->data, frame.data + i * SUB_FRAME_SIZE, SUB_FRAME_SIZE);
            GlobalAddress<SubFrame> this_subframe_addr = make_global(this_sf.get());

            GlobalAddress<SubFrame> sf_addr = delegate::call(video_server_core, [=]
                                                             {
                std::shared_ptr<SubFrame> sf = std::make_shared<SubFrame>();
                GlobalAddress<SubFrame> sf_addr = make_global(sf.get());
                return sf_addr; });
            Grappa::memcpy(sf_addr, this_subframe_addr, 1);
            global_free(sf_addr);
            global_free(this_subframe_addr);
        }
        // std::cout << "Process frame " << i << " of video " << video_id << " on core " << mycore() << " done" << std::endl;

        // std::cout << "Process frame of video " << video_id << " on core " << mycore() << " done" << std::endl;
    }

    size_t preview_id = delegate::call(video_server_core, [=]
                                       {
        // generate random id as stored video id
        std::random_device rd;  // Non-deterministic random number generator
        std::mt19937 rng(rd()); // Mersenne Twister RNG

        // Uniform distribution in range [0, 1000000000000000000)
        std::uniform_int_distribution<size_t> range(0, 1000000000000000000ULL);

        // Generate a random number within the specified range
        size_t id = range(rng);
        return id; });

    return std::make_tuple(width, height, preview_id);
}

void store_post(const Post &post)
{
    size_t post_key = post.post_id % 10000000000;
    post_cache[post_key] = post;

    // Optional: Print confirmation for demonstration
    // std::cout << "Post stored with ID: " << post.post_id << std::endl;
}

void compose_post(size_t req_id, const std::string &username, size_t user_id, std::string text, size_t media_id, const std::string &media_type, uint8_t post_type)
{
    double task_start = walltime();
    std::cout << "Task " << req_id << " on core " << mycore() << " locale " << mylocale() << " start" << std::endl << std::flush;

    // unique id
    Core unique_id_core = req_id % 16; // unique id run on the first 16 cores
    size_t unique_id = delegate::call(unique_id_core, [=]
                                      {
        size_t uid = generate_unique_id();
        return uid; });

    // std::cout << "Task " << req_id << " on core " << mycore() << " locale " << mylocale() << " unique id " << unique_id << std::endl;
    size_t video_id = unique_id;

    uint32_t _width = FRAME_WIDTH;
    uint32_t _height = FRAME_HEIGHT;

    // read preview_width, preview_height, preview_id from process_video
    uint32_t preview_width, preview_height;
    size_t preview_id;
    std::tie(preview_width, preview_height, preview_id) = process_video(_width, _height, video_id);

    // std::cout << "Task " << req_id << " on core " << mycore() << " locale " << mylocale() << " preview id " << preview_id << std::endl;
    ProcessedText processed_text = process_text(text);
    // get text, mentions, utls from processed_text
    std::string text_str = processed_text.text;
    std::vector<std::string> mentions = processed_text.mentions;
    std::vector<std::string> urls = processed_text.urls;

    auto now = std::chrono::system_clock::now();
    auto duration_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

    // std::cout << "Task " << req_id << " on core " << mycore() << " locale " << mylocale() << " text_str " << text_str << std::endl;
    // store post
    Post post;
    post.post_id = unique_id;
    post.req_id = req_id;
    post.text = text_str;
    post.mentions = mentions;
    post.preview_width = preview_width;
    post.preview_height = preview_height;
    post.preview = preview_id;
    post.urls = urls;
    post.timestamp = duration_since_epoch.count();
    post.post_type = post_type;

    store_post(post);

    // std::cout << "Task " << req_id << " on core " << mycore() << " preview id " << preview_id << std::endl;
    // std::cout << "Task " << req_id << " on core " << mycore() << " text_str " << text_str << std::endl;
    std::cout << "Task " << req_id << " on core " << mycore() << " on locale " << mylocale() << " takes " << walltime() - task_start << std::endl << std::flush;
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
            std::cout << "Video decoding takes " << walltime() - video_start << std::endl << std::flush;
            // run socialnet benchmark on root task (core 0)

            // gam version is a async function, may change later
            double start = walltime();
            double t_st = start;
            double t_ed_mid = start;


            for(int i = 0; i < 9; ++i) {
                finish([=]{
                    forall<TaskMode::Unbound, SyncMode::Async>(i*2048, 2048, [](int64_t id){
                        compose_post(id, "username", id, "0 1467810672 Mon Apr 06 22:19:49 PDT 2009 NO_QUERY scotthamilton is upset that he can't update his Facebook by texting it... and might cry as a result  School today also. Blah!",  id, "video",  static_cast<uint8_t>(0));
                    });
                });

                if(i == 1){
                    t_ed_mid = walltime();
                }
                std::cout << "Iteration " << i << " takes " << walltime() - t_st << std::endl;
                t_st = walltime();
            }

            std::ofstream myFile;
            int core_num = cores();
            char logname[256] = "/mnt/ssd/guest/DRust_home/logs/sn_grappa_";
            int length = strlen(logname);
            logname[length] = '0' + (core_num/16);
            logname[length+1] = '.';
            logname[length+2] = 't';
            logname[length+3] = 'x';
            logname[length+4] = 't';
            logname[length+5] = 0;
            myFile.open(logname);
            myFile << walltime() - start;
            myFile.close();

            std::cout << "Elapsed time: " << walltime() - start << std::endl;
            std::cout << "Half time: " << walltime() - t_ed_mid << std::endl;
        std::cout << "***** test video done *****" << std::endl; });

    finalize();
    return 0;
}
