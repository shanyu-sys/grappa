use rand::{thread_rng, distributions::Uniform, prelude::Distribution};
use serde::{Serialize, Deserialize};
use tokio::runtime::Runtime;
use std::{fmt, fs::File, io::{BufReader, BufRead}, sync::{Mutex, Once}, time::{SystemTime, Instant, UNIX_EPOCH}, net::{SocketAddr, IpAddr, Ipv4Addr}, time::Duration, io, ptr, sync::Arc, mem, thread};
use futures::{future, prelude::*};
use tokio::time::sleep;
use crate::drun_server;
use crate::conf::*;
use tarpc::{
    client,
    context,
    server::{self, incoming::Incoming, Channel},
    tokio_serde::formats::Json,
};
use dashmap::DashMap;
use image::{Rgb, RgbImage};
use imageproc::{map::map_colors, drawing::{draw_text_mut, text_size}};
use imageproc::pixelops::weighted_sum;
use rusttype::{Font, Scale};

use super::utils::*;
use super::video::decode;

pub const UNIQUE_ID_SERVER_ID: usize = 0;

static mut UNIQUE_ID_CACHE: Option<Arc<DashMap<usize, bool>>> = None;
static mut POST_CACHE: Option<Arc<DashMap<usize, Post>>> = None;
static INIT_UNIQUEID: Once = Once::new();
static INIT_POST: Once = Once::new();

pub static mut VIDEOS: Option<Arc<Vec<Video>>> = None;


async fn compose_post(req_id: usize, username: String, user_id: usize, mut text: String, media_id: usize, media_type: String, post_type: u8) {
    // print!("compose post");
    let mut ctx = context::current();
    ctx.deadline = SystemTime::now() + Duration::from_secs(RPC_WAIT);

    let _width = FRAME_WIDTH;
    let _height = FRAME_HEIGHT;

    let uniqueid_client = get_dclient(0);
    let unique_id = uniqueid_client.unique_id(ctx).await.unwrap();

    let mut video_id = unique_id;


    let preview_handle = tokio::spawn(async move {
        let compute_idx = get_compute().await;
        let video_client = get_dclient(compute_idx % NUM_SERVERS);
        let r = video_client.process_video(ctx, _width, _height, video_id).await;
        release_compute(compute_idx).await;
        r
    });
    let transfer_text = text.clone();
    let text_handle = tokio::spawn(async move {
        let compute_idx = get_compute().await;
        let text_client = get_dclient(compute_idx % NUM_SERVERS);
        let r = text_client.process_text(ctx, transfer_text).await;
        release_compute(compute_idx).await;
        r
    });
    
    let (processed_text, mentions, urls) = text_handle.await.unwrap().unwrap();
    let (preview_width, preview_height, preview) = preview_handle.await.unwrap().unwrap();

    
    // let preview = transfer_media;
    let now = SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_millis();
    let post = Post {
        post_id: unique_id,
        req_id,
        text: processed_text,
        mentions,
        preview_width,
        preview_height,
        preview,
        urls,
        timestamp: now,
        post_type,
    };
    let compute_idx = get_compute().await;

    let store_client = get_dclient(compute_idx % NUM_SERVERS);
    store_client.store_post(ctx, post).await.unwrap();
    release_compute(compute_idx).await;
}

pub struct Request {
    pub req_id: usize,
    pub username: String,
    pub user_id: usize,
    pub text: String,
    pub media: Video,
    pub media_id: usize,
    pub media_type: String,
    pub post_type: u8,
}

pub async fn run_server(server_addr: SocketAddr) {
    drun_server!(server_addr, GAMSocialNetServer);
}

pub static mut DCLIENTS: Option<Vec<Arc<GAMSocialNetClient>>> = None;
pub fn get_dclient(server_idx: usize) -> Arc<GAMSocialNetClient> {
    unsafe{Arc::clone(DCLIENTS.as_ref().unwrap().get(server_idx).unwrap())}
}






/// Tint a grayscale value with the given color.
/// Midtones are tinted most heavily.
fn tint(gray: Rgb<u8>, color: Rgb<u8>) -> Rgb<u8> {
    let dist_from_mid = ((gray[0] as f32 - 128f32).abs()) / 255f32;
    let scale_factor = 1f32 - 4f32 * dist_from_mid.powi(2);
    weighted_sum(Rgb([gray[0]; 3]), color, 1.0, scale_factor)
}

/// Linearly interpolates between low and mid colors for pixel intensities less than
/// half of maximum brightness and between mid and high for those above.
fn color_gradient(gray: Rgb<u8>, low: Rgb<u8>, mid: Rgb<u8>, high: Rgb<u8>) -> Rgb<u8> {
    let fraction = gray[0] as f32 / 255f32;
    let (lower, upper, offset) = if fraction < 0.5 {
        (low, mid, 0.0)
    } else {
        (mid, high, 0.5)
    };
    let right_weight = 2.0 * (fraction - offset);
    let left_weight = 1.0 - right_weight;
    weighted_sum(lower, upper, left_weight, right_weight)
}


fn process_frame(frame: Vec<u8>) -> Vec<u8> {
    let mut image: image::ImageBuffer<Rgb<u8>, Vec<u8>> = image::ImageBuffer::from_raw(FRAME_WIDTH, FRAME_HEIGHT, frame.clone()).unwrap();
    let blue = Rgb([0u8, 0u8, 255u8]);
    let black = Rgb([0u8, 0u8, 0u8]);
    let red = Rgb([255u8, 0u8, 0u8]);
    let yellow = Rgb([255u8, 255u8, 0u8]);

    image = map_colors(&image, |pix| tint(pix, blue));
    image = map_colors(&image, |pix| color_gradient(pix, black, red, yellow));

    let font = Vec::from(include_bytes!("DejaVuSans.ttf") as &[u8]);
    let font = Font::try_from_vec(font).unwrap();

    let height = 20.0;
    let scale = Scale {
        x: height * 2.0,
        y: height,
    };
    let text = "Hello, world!";
    draw_text_mut(&mut image, Rgb([0u8, 0u8, 255u8]), 0, 0, scale, &font, text);

    let text = "HollandMa!";
    draw_text_mut(&mut image, Rgb([0u8, 0u8, 255u8]), 0, 0, scale, &font, text);
    
    let image = map_colors(&image, |pix| tint(pix, blue));
    let image = map_colors(&image, |pix| color_gradient(pix, black, red, yellow));

    // Convert gradient to a vector of bytes
    let gradient_vec = image.into_raw();
    gradient_vec
}




#[tarpc::service]
pub trait GAMSocialNet {
    async fn get_video(video_id: usize) -> Video;
    async fn store_video(video: Video) -> usize;
    async fn process_video(width: u32, height: u32, video_id: usize) -> (u32, u32, usize);
    async fn process_text(text: String) -> (String, Vec<String>, Vec<String>);
    async fn store_post(post: Post);
    async fn unique_id() -> usize;
}



// This is the type that implements the generated World trait. It is the business logic
// and is used to start the server.
#[derive(Clone)]
struct GAMSocialNetServer(SocketAddr);

#[tarpc::server]
impl GAMSocialNet for GAMSocialNetServer {
    async fn get_video(self, _: context::Context, video_id: usize) -> Video {
        // println!("get video!!!!!!!");
        let videos = unsafe{Arc::clone(VIDEOS.as_ref().unwrap())};
        let local_video_id = video_id % videos.len();
        let r = videos[local_video_id].clone();
        r
    }

    async fn store_video(self, _: context::Context, video: Video) -> usize {
        let local_video = video.to_store_video();
        let mut id = 0;
        let mut rng = thread_rng();
        let range = Uniform::new(0, 1000000000000000000);
        id = range.sample(&mut rng);
        id
    }

    async fn process_video(self, _: context::Context, width: u32, height: u32, video_id: usize) ->  (u32, u32, usize) {
        let client_id = video_id % VIDEO_STORAGE_SERVER_NUM + VIDEO_STORAGE_SERVER_START;
        let client_ref = get_dclient(client_id);
        let mut ctx = context::current();
        ctx.deadline = SystemTime::now() + Duration::from_secs(RPC_WAIT);
        let video = client_ref.get_video(ctx, video_id).await.unwrap();
        let frame_ref = video.frames;
        let len = frame_ref.len();
        let mut filtered_frames: Vec<Frame> = Vec::with_capacity(len);
        let mut index = 0;
        while index < len {
            let mut frame_data = Vec::with_capacity(FRAME_SIZE);
            unsafe {
                let tid = get_resource();
                frame_data.set_len(FRAME_SIZE);
                gam_read(frame_ref[index].frame, frame_data.as_mut_ptr() as usize, FRAME_SIZE, 0, tid);
                release_resource(tid);
            }
            let processed_frame = process_frame(frame_data);
            unsafe {
                let tid = get_resource();
                let frame_ptr = gam_malloc(FRAME_SIZE, false, false, tid);
                gam_write(frame_ptr, processed_frame.as_ptr() as usize, FRAME_SIZE, 0, tid);
                gam_mfence(tid);
                release_resource(tid);
                let frame = Frame {
                    frame: frame_ptr,
                };
                filtered_frames.push(frame);
            }
            index += 1;
        }
        let preview_id = client_ref.store_video(ctx, Video {
            width,
            height,
            frames: filtered_frames,
        }).await.unwrap();
        let preview_id = 0;
        (width, height, preview_id)
    }
    async fn process_text(self, _: context::Context, mut text: String) -> (String, Vec<String>, Vec<String>) {
        // println!("Received text: {}", text);
        let mut mentions = Vec::new();
        let mut urls = Vec::new();
        let length = text.len();
        let mut i = 0;
        while i < length {
            if i < length - 8 && (text.as_bytes()[i..i+7] == "http://".as_bytes()[..] || text.as_bytes()[i..i+8] == "https://".as_bytes()[..]) {
                let mut j = i + 8;
                while j < length && text.as_bytes()[j] != b' ' {
                    j += 1;
                }
                urls.push(text[i..j].to_string());
                i = j;
                continue;
            }
            if text.as_bytes()[i] == b'@' {
                let mut j = i;
                while j < length && text.as_bytes()[j] != b' ' {
                    j += 1;
                }
                mentions.push(text[i..j].to_string());
                for k in i..j {
                    text.replace_range(k..k+1, "*")
                }
                i = j;
            }
            i += 1;
        }
        (text, mentions, urls)
    }

    async fn store_post(self, _: context::Context, post: Post) {
        INIT_POST.call_once(|| {
            unsafe{POST_CACHE = Some(Arc::new(DashMap::new()));}
        });
        // let store_post = StorePost::from_post(post);
        let post_cache = unsafe{Arc::clone(POST_CACHE.as_ref().unwrap())};
        post_cache.insert(post.post_id % 10000000000, post);
    }

    async fn unique_id(self, _: context::Context) -> usize {
        if unsafe{SERVER_INDEX != UNIQUE_ID_SERVER_ID} {
            println!("Wrong server id");
            return 0;
        }

        INIT_UNIQUEID.call_once(|| {
            unsafe{UNIQUE_ID_CACHE = Some(Arc::new(DashMap::new()));}
        });

        let mut rng = thread_rng();
        let range = Uniform::new(0, 1000000000000000000);
        let mut id = range.sample(&mut rng);
        let unique_id_cache = unsafe{Arc::clone(UNIQUE_ID_CACHE.as_ref().unwrap())};
        while unique_id_cache.contains_key(&id) {
            id = range.sample(&mut rng);
        }
        unique_id_cache.insert(id, true);
        id
    }
}

#[derive(Serialize, Deserialize)]
struct Server {
    ip: String,
    alloc_ip: String,
}
#[derive(Serialize, Deserialize)]
struct Config {
    num_servers: u32,
    servers: Vec<Server>,
}

pub fn get_server_addrs() -> ([SocketAddr; NUM_SERVERS], [SocketAddr; NUM_SERVERS]) {
    let config = serde_json::from_str::<Config>(&std::fs::read_to_string("../drust.json").unwrap()).unwrap();
    let mut server_addrs = [SocketAddr::from(([0, 0, 0, 0], 0)); NUM_SERVERS];
    let mut alloc_server_addr = [SocketAddr::from(([0, 0, 0, 0], 0)); NUM_SERVERS];
    for i in 0..NUM_SERVERS {
        server_addrs[i] = config.servers[i].ip.parse().unwrap();
        alloc_server_addr[i] = config.servers[i].alloc_ip.parse().unwrap();
    }
    (server_addrs, alloc_server_addr)
}


pub async fn socialnet_benchmark() {

    let start = SystemTime::now();
    let mut id = 0;
    let mut t_st = Duration::from_secs(0);
    for i in 0..4 {
        let mut handles = Vec::new();
        for id in (i * 5000)..(i * 5000 + 5000) {
            // println!("duplicate video id {}", id);
            handles.push(tokio::spawn(async move {
                compose_post(id, "username".to_string(), id, "0 1467810672 Mon Apr 06 22:19:49 PDT 2009 NO_QUERY scotthamilton is upset that he can't update his Facebook by texting it... and might cry as a result  School today also. Blah!".to_string(),  id, "video".to_string(), 0).await;
            }));
        }
        for handle in handles {
            handle.await.unwrap();
        }
        
        let time = start.elapsed().unwrap();
        if i == 1 {
            t_st = time;
        }
        println!("Round {} Elapsed Time: {:?}", i, time);
    }
    let time = start.elapsed().unwrap();
    println!("Elapsed Time: {:?}", time);
    println!("Half Time: {:?}", time - t_st);


}