use ffmpeg_next::format::{input, Pixel};
use ffmpeg_next::media::Type;
use ffmpeg_next::software::scaling::{context::Context, flag::Flags};
use ffmpeg_next::util::frame::video::Video as ffmpegVideo;
use crate::app::socialnet::utils::{FRAME_SIZE, Frame};
use crate::conf::*;
use super::utils::Video;

pub struct VideoLib {
    pub videos: Vec<Video>,
}

pub fn decode() -> Result<Video, ffmpeg_next::Error> {
    ffmpeg_next::init().unwrap();
    let mut a = Vec::new();
    let mut height = 0;
    let mut width = 0;

    if let Ok(mut ictx) = input(&"/mnt/ssd/haoran/types_proj/dataset/12svideo.mp4".to_string()) {
        let input = ictx
            .streams()
            .best(Type::Video)
            .ok_or(ffmpeg_next::Error::StreamNotFound)?;
        let video_stream_index = input.index();

        let context_decoder = ffmpeg_next::codec::context::Context::from_parameters(input.parameters())?;
        let mut decoder = context_decoder.decoder().video()?;

        let mut scaler = Context::get(
            decoder.format(),
            decoder.width(),
            decoder.height(),
            Pixel::RGB24,
            decoder.width()/4,
            decoder.height()/4,
            Flags::BILINEAR,
        )?;
        height = decoder.height();
        width = decoder.width();
        let mut frame_index = 0;

        let mut receive_and_process_decoded_frames =
            |decoder: &mut ffmpeg_next::decoder::Video| -> Result<(), ffmpeg_next::Error> {
                let mut decoded = ffmpegVideo::empty();
                while decoder.receive_frame(&mut decoded).is_ok() {
                    let mut rgb_frame = ffmpegVideo::empty();
                    scaler.run(&decoded, &mut rgb_frame)?;

                    // save_file(&rgb_frame, frame_index).unwrap();
                    if frame_index < 5 {
                        let mut frame_vec = Vec::with_capacity(FRAME_SIZE);
                        frame_vec.append(&mut rgb_frame.data(0).to_vec());
                        let (ptr, len, cap) = frame_vec.into_raw_parts();
                        if len != FRAME_SIZE || cap != FRAME_SIZE {
                            println!("len: {}, cap: {}", len, cap);
                            panic!("frame size not match");
                        }
                        let tid = get_resource();
                        let gam_ptr = unsafe { gam_malloc(FRAME_SIZE, false, false, tid) };
                        println!("gam_ptr: {}", gam_ptr);
                        
                        unsafe{gam_write(gam_ptr, ptr as usize, FRAME_SIZE, 0, tid);}
                        unsafe{gam_mfence(tid);}
                        println!("after write: {}, buf {}", gam_ptr, ptr as usize);
                        release_resource(tid);
                        unsafe{frame_vec = Vec::from_raw_parts(ptr, len, cap);}
                        let frame = Frame{frame: gam_ptr};
                        a.push(frame);
                    }
                    // println!("frame length: {}", rgb_frame.data(0).len());
                    frame_index += 1;
                }
                Ok(())
            };
        for (stream, packet) in ictx.packets() {
            if stream.index() == video_stream_index {
                decoder.send_packet(&packet)?;
                receive_and_process_decoded_frames(&mut decoder)?;
            }
        }
        decoder.send_eof()?;
        receive_and_process_decoded_frames(&mut decoder)?;
    }

    // print!("{} ", a.len());

    Ok(Video { width, height, frames: a })
}


