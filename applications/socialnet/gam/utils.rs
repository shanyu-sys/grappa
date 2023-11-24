use arr_macro::arr;
use tarpc::{
  context,
  server::{self, incoming::Incoming, Channel},
  tokio_serde::formats::Json, client,
};
use serde::{Serialize, Deserialize};
use std::{net::SocketAddr, thread, sync::atomic::AtomicBool};
use crate::conf::*;

// pub const FRAME_HEIGHT: u32 = 144;
pub const FRAME_HEIGHT: u32 = 1080 / 4;
pub const FRAME_WIDTH: u32 = 1920 / 4;
pub const FRAME_SIZE: usize = FRAME_HEIGHT as usize * FRAME_WIDTH as usize * 3;

pub const VIDEO_STORAGE_SERVER_NUM: usize = 2;
pub const VIDEO_STORAGE_SERVER_START: usize = 2;

#[derive(Clone, Serialize, Deserialize, Debug)]
pub struct Frame {
    pub frame: usize,
}

impl Frame {
    fn deep_clone(&self) -> Self {
        let mut buffer = vec![0u8; FRAME_SIZE];
        let (box_ptr, _len, _cap) = buffer.into_raw_parts();
        let ptr = unsafe{
            let tid = get_resource();
            let mut ptr = gam_malloc(FRAME_SIZE, false, false, tid);
            gam_read(self.frame, box_ptr as usize, FRAME_SIZE, 0, tid);
            gam_write(ptr, box_ptr as usize, FRAME_SIZE, 0, tid);
            gam_mfence(tid);
            // println!("deepclone read write finish: tid: {}, tid2: {}", tid, tid2);
            release_resource(tid);

            ptr
        };
        buffer = unsafe{Vec::from_raw_parts(box_ptr, _len, _cap)};
        Frame {
            frame: ptr,
        }
    }
}

#[derive(Clone,Serialize, Deserialize, Debug)]
pub struct Video {
    pub width: u32,
    pub height: u32,
    pub frames: Vec<Frame>,
}


pub struct StoreVideo {
    pub width: u32,
    pub height: u32,
    pub frames: Vec<Vec<u8>>,
}

impl Video {
    pub fn deep_clone(&self) -> Self {
        let mut frames = Vec::with_capacity(self.frames.len());
        for frame in self.frames.iter() {
            frames.push(frame.deep_clone());
        }
        Video {
            width: self.width,
            height: self.height,
            frames,
        }
    }

    pub fn to_store_video(self) -> StoreVideo {
        let mut frames = Vec::with_capacity(self.frames.len());
        for frame in self.frames.iter() {
            let mut buffer = vec![0u8; FRAME_SIZE];
            let (box_ptr, _len, _cap) = buffer.into_raw_parts();
            unsafe{
                let tid = get_resource();
                // gam_mfence(tid);
                // println!("start read frame: {}", frame.frame);
                gam_read(frame.frame, box_ptr as usize, FRAME_SIZE, 0, tid);
                gam_free(frame.frame, tid);
                release_resource(tid);
            }
            buffer = unsafe{Vec::from_raw_parts(box_ptr, _len, _cap)};
            frames.push(buffer);
        }
        StoreVideo {
            width: self.width,
            height: self.height,
            frames,
        }
    }
}

#[derive(Clone,Serialize, Deserialize, Debug)]
pub struct Post {
    pub post_id: usize,
    pub req_id: usize,
    pub text: String,
    pub mentions: Vec<String>,
    pub preview_width: u32,
    pub preview_height: u32,
    pub preview: usize,
    pub urls: Vec<String>,
    pub timestamp: u128,
    pub post_type: u8,
}


// pub const COMPUTE_NUM: usize = NUM_SERVERS * 32; // socialnet = 1 2
pub const COMPUTE_NUM: usize = NUM_SERVERS * 64; // socialnet > 2
pub static COMPUTE_AVAILABLE: [AtomicBool; 512] = arr![AtomicBool::new(true); 512];
pub async fn get_compute() -> usize {
    let tid = std::thread::current().id().as_u64().get() as usize % (COMPUTE_NUM as usize);
    let mut rem = tid;
    loop {
        if COMPUTE_AVAILABLE[rem].compare_exchange(true, false, std::sync::atomic::Ordering::SeqCst, std::sync::atomic::Ordering::SeqCst).is_ok(){
            break;
        }
        rem = (rem + 1) % (COMPUTE_NUM as usize);
        if rem == tid {
            
            tokio::time::sleep(std::time::Duration::from_millis(5)).await; // socialnet == 8

            // tokio::time::sleep(std::time::Duration::from_millis(100/NUM_SERVERS as u64)).await; // socialnet >= 2
            // tokio::time::sleep(std::time::Duration::from_millis(100)).await; // socialnet = 1
        }
    }
    rem
}
pub async fn release_compute(resource_id: usize) {
    COMPUTE_AVAILABLE[resource_id].store(true, std::sync::atomic::Ordering::SeqCst);
}