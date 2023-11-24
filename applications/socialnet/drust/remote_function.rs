use std::{ net::SocketAddr, sync::{Arc, Once} };
use dashmap::DashMap;
use good_memory_allocator::SpinLockedAllocator;
use rand::{thread_rng, distributions::Uniform, prelude::Distribution};
use tarpc::{
    context,
    server::{ self, incoming::Incoming, Channel },
    tokio_serde::formats::Json,
};
use futures::{ future, prelude::* };
use crate::prelude::*;
use super::utils::*;
use image::{Rgb, RgbImage};
use imageproc::{map::map_colors, drawing::{draw_text_mut, text_size}};
use imageproc::pixelops::weighted_sum;
use rusttype::{Font, Scale};

pub const UNIQUE_ID_SERVER_ID: usize = 0;

static mut UNIQUE_ID_CACHE: Option<Arc<DashMap<usize, bool>>> = None;
static mut POST_CACHE: Option<Arc<DashMap<usize, Post>>> = None;
static INIT_UNIQUEID: Once = Once::new();
static INIT_POST: Once = Once::new();


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


fn process_frame(frame: &mut Frame) {
    let frame_vec = frame.frame.to_vec();
    let image: image::ImageBuffer<Rgb<u8>, Vec<u8>> = image::ImageBuffer::from_raw(FRAME_WIDTH, FRAME_HEIGHT, frame_vec).unwrap();
    let blue = Rgb([0u8, 0u8, 255u8]);
    // // Apply the color tint to every pixel in the grayscale image, producing a image::RgbImage
    // let tinted = map_colors(&image, |pix| tint(pix, blue));
    // Apply color gradient to each image pixel
    let black = Rgb([0u8, 0u8, 0u8]);
    let red = Rgb([255u8, 0u8, 0u8]);
    let yellow = Rgb([255u8, 255u8, 0u8]);

    let image = map_colors(&image, |pix| tint(pix, blue));
    let mut image = map_colors(&image, |pix| color_gradient(pix, black, red, yellow));

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
    // Convert vector of bytes back to frame
    let addr = frame.frame.get_addr();
    unsafe{std::ptr::copy_nonoverlapping(gradient_vec.as_ptr(), addr as *mut u8, FRAME_SIZE)};
}


#[tarpc::service]
pub trait DrustSocialNet {
    async fn test();
    async fn process_video(width: u32, height: u32, frame_ref_wrapper: DVecRefWrapper) -> (u32, u32, DVecWrapper);
    async fn process_text(text: String) -> (String, Vec<String>, Vec<String>);
    async fn store_post(post_wrapper: PostWrapper);
    async fn unique_id() -> usize;
}


#[derive(Clone)]
struct DrustSocialNetServer(SocketAddr);

#[tarpc::server]
impl DrustSocialNet for DrustSocialNetServer {
    async fn test(self, _: context::Context) {
        println!("test");
    }

    async fn process_video(self, _: context::Context, width: u32, height: u32, frame_ref_wrapper: DVecRefWrapper) -> (u32, u32, DVecWrapper) {
        // println!("process video width: {}, height: {}, frame_ref_wrapper: {:?}", width, height, frame_ref_wrapper);
        let mut dvec_ref: DVecRef<'_, Frame> = frame_ref_wrapper.from_wrapper();
        // println!("original raw: {:?}", dvec_ref.orig_raw);
        let dvec_regular_ref = dvec_ref.as_regular();
        // println!("len: {}", dvec_regular_ref.len());
        let len = dvec_regular_ref.len();
        let mut filtered_frames: Vec<Frame, &SpinLockedAllocator> = unsafe{Vec::with_capacity_in((len+9)/10, &LOCAL_ALLOCATOR)};
        let mut index = 0;
        while index < len {
            let addr = dvec_regular_ref[index].frame.get_addr();
            if addr < GLOBAL_HEAP_START || addr >= GLOBAL_HEAP_START + WORKER_HEAP_SIZE{
                println!("index: {}, addr: {}", index, addr);
                for i in 0..len {
                    println!("index: {}, addr: {}", i, dvec_regular_ref[i].frame.get_addr());
                }
                panic!("addr not in global heap");
            }
            let frame_data = dvec_regular_ref[index].frame.clone();
            let mut frame = Frame {
                frame: frame_data,
            };
            process_frame(&mut frame);
            filtered_frames.push(frame);
            index += 1;
        }
        drop(dvec_regular_ref);
        drop(dvec_ref);
        (width, height, DVecWrapper::to_wrapper(unsafe{DVec::from_raw(filtered_frames)}))
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

    async fn store_post(self, _: context::Context, post_wrapper: PostWrapper) {
        INIT_POST.call_once(|| {
            unsafe{POST_CACHE = Some(Arc::new(DashMap::new()));}
        });
        let post = Post::from_wrapper(post_wrapper);
        let post_cache = unsafe{Arc::clone(POST_CACHE.as_ref().unwrap())};
        post_cache.insert(post.post_id % 3000, post);
    }

    async fn unique_id(self, _: context::Context) -> usize {
        if unsafe{SERVER_INDEX} != unsafe{UNIQUE_ID_SERVER_ID} {
            println!("Wrong server for unique id");
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

pub async fn run_server(server_addr: SocketAddr) {
    drun_server!(server_addr, DrustSocialNetServer);
}


pub static mut DCLIENTS: Option<Vec<Arc<DrustSocialNetClient>>> = None;
pub fn get_dclient(server_idx: usize) -> Arc<DrustSocialNetClient> {
    unsafe{Arc::clone(DCLIENTS.as_ref().unwrap().get(server_idx).unwrap())}
}