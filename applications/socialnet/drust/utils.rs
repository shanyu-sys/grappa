use arr_macro::arr;
use tarpc::{
    context,
    server::{self, incoming::Incoming, Channel},
    tokio_serde::formats::Json, client,
};
use serde::{Serialize, Deserialize};
use std::{net::SocketAddr, sync::atomic::AtomicBool};
use crate::{
    prelude::*,
    exclude,
};
// pub const FRAME_HEIGHT: u32 = 144;
pub const FRAME_HEIGHT: u32 = 1080 / 4;
pub const FRAME_WIDTH: u32 = 1920 / 4;
pub const FRAME_SIZE: usize = FRAME_HEIGHT as usize * FRAME_WIDTH as usize * 3;

#[derive(Clone,Serialize, Deserialize, Debug)]
pub struct FrameWrapper {
    pub frame: usize,
}
pub struct Frame { pub frame: DBox<[u8; FRAME_SIZE]>,
}
exclude!([u8; FRAME_SIZE], 4);
exclude!(Frame, 5);


pub struct Video {
    pub width: u32,
    pub height: u32,
    pub frames: DVec<Frame>,
}

impl Clone for Video {
    fn clone(&self) -> Self {
        let frames_ref = unsafe{self.frames.as_local_ref()};
        let mut new_frame_vec = unsafe{Vec::with_capacity_in(frames_ref.len(), &LOCAL_ALLOCATOR)};

        for frame in frames_ref {
            let local_frame = frame.frame.clone();
            new_frame_vec.push(Frame {
                frame: local_frame,
            });
        }
        Video {
            width: self.width,
            height: self.height,
            frames: unsafe{DVec::from_raw(new_frame_vec)},
        }
    }
}


#[derive(Clone,Serialize, Deserialize, Debug)]
pub struct PostWrapper {
    pub post_id: usize,
    pub req_id: usize,
    pub text: String,
    pub mentions: Vec<String>,
    pub media_width: u32,
    pub media_height: u32,
    pub media: DVecRefWrapper,
    pub preview_width: u32,
    pub preview_height: u32,
    pub preview: DVecWrapper,
    pub urls: Vec<String>,
    pub timestamp: u128,
    pub post_type: u8,
}

pub struct Post {
    pub post_id: usize,
    pub req_id: usize,
    pub text: String,
    pub mentions: Vec<String>,
    pub media_width: u32,
    pub media_height: u32,
    pub media: Vec<Vec<u8>>,
    pub preview_width: u32,
    pub preview_height: u32,
    pub preview: Vec<Vec<u8>>,
    pub urls: Vec<String>,
    pub timestamp: u128,
    pub post_type: u8,
}

impl Post {
    pub fn from_wrapper(wrapper: PostWrapper) -> Self {
        let mut media_vec: DVecRef<Frame> = DVecRefWrapper::from_wrapper(&wrapper.media);
        let media_vec_ref = media_vec.as_regular();
        let mut media_frames = Vec::with_capacity(media_vec_ref.len());
        for frame in media_vec_ref {
            // frame.frame.migrate_to_local();
            let local_frame = frame.frame.clone();
            let mut frame_data = Vec::with_capacity(FRAME_SIZE);
            unsafe{
                std::ptr::copy_nonoverlapping(&(*(local_frame)) as *const u8, frame_data.as_mut_ptr(), FRAME_SIZE);
                frame_data.set_len(FRAME_SIZE);
            }
            for i in 0..FRAME_SIZE {
                frame_data[i] -= 1;
            }
            media_frames.push(frame_data);
        }
        let mut preview_vec: DVec<Frame> = DVecWrapper::from_wrapper(wrapper.preview);
        preview_vec.migrate_to_local();
        let preview_vec_ref = unsafe{preview_vec.as_local_mut_ref()};
        let mut preview_frames = Vec::with_capacity(preview_vec_ref.len());
        for frame in preview_vec_ref {
            frame.frame.migrate_to_local();
            let mut frame_data = Vec::with_capacity(FRAME_SIZE);
            unsafe{
                std::ptr::copy_nonoverlapping(&(*(frame.frame)) as *const u8, frame_data.as_mut_ptr(), FRAME_SIZE);
                frame_data.set_len(FRAME_SIZE);
            }
            preview_frames.push(frame_data);
        }
        Post {
            post_id: wrapper.post_id,
            req_id: wrapper.req_id,
            text: wrapper.text,
            mentions: wrapper.mentions,
            media_width: wrapper.media_width,
            media_height: wrapper.media_height,
            media: media_frames,
            preview_width: wrapper.preview_width,
            preview_height: wrapper.preview_height,
            preview: preview_frames,
            urls: wrapper.urls,
            timestamp: wrapper.timestamp,
            post_type: wrapper.post_type,
        }
    }
}


pub const COMPUTE_NUM: usize = NUM_SERVERS * 64; //evaluated in this number
// pub const COMPUTE_NUM: usize = NUM_SERVERS * 32; // new number
pub static COMPUTE_AVAILABLE: [AtomicBool; 512] = arr![AtomicBool::new(true); 512];
pub async fn get_compute() -> usize {
    let tid = std::thread::current().id().as_u64().get() as usize;
    let mut rem = tid % (COMPUTE_NUM as usize);
    loop {
        if COMPUTE_AVAILABLE[rem].compare_exchange(true, false, std::sync::atomic::Ordering::SeqCst, std::sync::atomic::Ordering::SeqCst).is_ok(){
            break;
        }
        rem = (rem + 1) % (COMPUTE_NUM as usize);
        if rem == 0 {
            tokio::time::sleep(std::time::Duration::from_millis(1)).await;
        }
    }
    rem
}
pub async fn release_compute(resource_id: usize) {
    COMPUTE_AVAILABLE[resource_id].store(true, std::sync::atomic::Ordering::SeqCst);
}