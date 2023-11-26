#include "video.hpp"
#include <vector>
#include "constants.h"
#include <Grappa.hpp>
#include <Delegate.hpp>
#include "frame.hpp"

using namespace Grappa;

int main(int argc, char *argv[])
{
    init(&argc, &argv);

    run([]
        {
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
        std::cout << "***** test video done *****" << std::endl; });

    finalize();
    return 0;
}
