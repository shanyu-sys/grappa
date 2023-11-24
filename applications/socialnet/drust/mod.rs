pub mod utils;
pub mod video;
pub mod remote_function;


use std::{net::SocketAddr, io, time::{SystemTime, Duration, UNIX_EPOCH}, sync::{Arc, atomic::AtomicU64}};
use tarpc::{client::{self, Config}, context, tokio_serde::formats::Json};
use tokio::runtime::Runtime;

use crate::prelude::*;
use remote_function::*;
use video::*;
use utils::*;

async fn compose_post(req_id: usize, username: String, user_id: usize, mut text: String, media: Arc<Video>, media_id: usize, media_type: String, post_type: u8) {
    // print!("compose post");
    
    let mut ctx = context::current();
    ctx.deadline = SystemTime::now() + Duration::from_secs(RPC_WAIT);
    let transfer_media = DVecRefWrapper::to_wrapper(&media.frames);
    let _width = media.width;
    let _height = media.height;
    let preview_handle = tokio::spawn(async move {
        let compute_idx = get_compute().await;
        let video_client = get_dclient(compute_idx % NUM_SERVERS);
        let r = video_client.process_video(ctx, _width, _height, transfer_media).await;
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
    
    let uniqueid_client = get_dclient(0);
    let id_handle = tokio::spawn(async move {uniqueid_client.unique_id(ctx).await});
    let unique_id = id_handle.await.unwrap().unwrap();
    let (processed_text, mentions, urls) = text_handle.await.unwrap().unwrap();
    let (preview_width, preview_height, preview) = preview_handle.await.unwrap().unwrap();
    // let mut preview: DVec<Frame> = preview.from_wrapper();
    // preview.migrate_to_local();
    // let preview_ref = unsafe{preview.as_local_mut_ref()};
    // for frame in preview_ref {
    //     frame.frame.migrate_to_local();
    // }


    let store_media = DVecRefWrapper::to_wrapper(&(media.frames));
    let now = SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_millis();
    let post = PostWrapper {
        post_id: unique_id,
        req_id,
        text: processed_text,
        mentions,
        media_width: media.width,
        media_height: media.height,
        media: store_media,
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


// async fn compose_post(req_id: usize, username: String, user_id: usize, mut text: String, media: Arc<Video>, media_id: usize, media_type: String, post_type: u8, video_client: Arc<DrustSocialNetClient>, text_client: Arc<DrustSocialNetClient>, store_client: Arc<DrustSocialNetClient>, uniqueid_client: Arc<DrustSocialNetClient>) {
//     // print!("compose post");
    
//     let mut ctx = context::current();
//     ctx.deadline = SystemTime::now() + Duration::from_secs(RPC_WAIT);
//     let transfer_media = DVecRefWrapper::to_wrapper(&media.frames);
//     let _width = media.width;
//     let _height = media.height;
//     let preview_handle = tokio::spawn(async move {video_client.process_video(ctx, _width, _height, transfer_media).await});
//     let transfer_text = text.clone();
//     let text_handle = tokio::spawn(async move {text_client.process_text(ctx, transfer_text).await});
//     let id_handle = tokio::spawn(async move {uniqueid_client.unique_id(ctx).await});
//     let unique_id = id_handle.await.unwrap().unwrap();
//     let (processed_text, mentions, urls) = text_handle.await.unwrap().unwrap();
//     let (preview_width, preview_height, preview) = preview_handle.await.unwrap().unwrap();
//     // let mut preview: DVec<Frame> = preview.from_wrapper();
//     // preview.migrate_to_local();
//     // let preview_ref = unsafe{preview.as_local_mut_ref()};
//     // for frame in preview_ref {
//     //     frame.frame.migrate_to_local();
//     // }


//     let store_media = DVecRefWrapper::to_wrapper(&(media.frames));
//     let now = SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_millis();
//     let post = PostWrapper {
//         post_id: unique_id,
//         req_id,
//         text: processed_text,
//         mentions,
//         media_width: media.width,
//         media_height: media.height,
//         media: store_media,
//         preview_width,
//         preview_height,
//         preview,
//         urls,
//         timestamp: now,
//         post_type,
//     };
//     store_client.store_post(ctx, post).await.unwrap();
// }

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


pub async fn socialnet_benchmark() {
    let video = decode().unwrap();
    let video_ref = Arc::new(video);

    let start = SystemTime::now();
    let mut id = 0;
    for i in 0..4{
        let mut handles = Vec::new();
        for id in (i * 5000)..(i * 5000 + 5000) {
            // let uniqueid_client = get_dclient(0);
            // let text_client = get_dclient(0);
            // let (video_client, store_client) = if NUM_SERVERS > 1 {
            //     // (get_dclient(id % (NUM_SERVERS)), get_dclient((id + 1) % (NUM_SERVERS)))
            //     (get_dclient(id % 7 / (NUM_SERVERS + 2)), get_dclient(id % 7 / (NUM_SERVERS + 2)))
            // } else {
            //     (get_dclient(0), get_dclient(0))
            // };
            // let video_arc = Arc::clone(&video_ref);
            // handles.push(tokio::spawn(async move {
            //     compose_post(id, "username".to_string(), id, "0 1467810672 Mon Apr 06 22:19:49 PDT 2009 NO_QUERY scotthamilton is upset that he can't update his Facebook by texting it... and might cry as a result  School today also. Blah!".to_string(), video_arc, id, "video".to_string(), 0, video_client, text_client, store_client, uniqueid_client).await;
            // }));
            let video_arc = Arc::clone(&video_ref);
            handles.push(tokio::spawn(async move {
                compose_post(id, "username".to_string(), id, "0 1467810672 Mon Apr 06 22:19:49 PDT 2009 NO_QUERY scotthamilton is upset that he can't update his Facebook by texting it... and might cry as a result  School today also. Blah!".to_string(), video_arc, id, "video".to_string(), 0).await;
            }));
        }
        println!("id {}", id);
        for handle in handles {
            handle.await.unwrap();
        }
    }
    let time = start.elapsed().unwrap();
    println!("Elapsed Time: {:?}", time);
}

pub async fn run(server_addr: [SocketAddr; NUM_SERVERS], server_idx: usize) {
    
    std::thread::spawn(move || {
        Runtime::new().unwrap().block_on(run_server(server_addr[server_idx]));
    });

    let mut guess = String::new();
    println!("RDMA connected, press enter to initialize the application clients");
    io::stdin().read_line(&mut guess).expect("failed to readline");

    dconnect!(server_addr, DCLIENTS, DrustSocialNetClient);


    if server_idx == 0 {
        socialnet_benchmark().await;
    }
}
