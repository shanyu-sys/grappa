#include "video.hpp"
#include <vector>
#include "constants.h"
#include <Grappa.hpp>
#include <Delegate.hpp>


using namespace Grappa;


// std::vector<Video> videos;

int main(int argc, char *argv[])
{
    init(&argc, &argv);

    run([]{
        Video v = decode();
        std::cout << "video size" << sizeof(v) << std::endl;
        // videos.emplace_back(v);

        // GlobalAddress<Frame> frame1 = delegate::call(1, []{
        // return videos[0].frames;
        // });

        // Frame f = delegate::read(frame1 + 3);
        // // check frame size
        // if(sizeof(f.data) != FRAME_SIZE){
        //     std::cout << "read back frame size mismatch" << std::endl;
        // }

        // // save frame to disk
        // cv::Mat image(FRAME_HEIGHT, FRAME_HEIGHT, CV_8UC3, f.data);
        // cv::imwrite("frame.png", image);

    });

    finalize();
    return 0;
}
