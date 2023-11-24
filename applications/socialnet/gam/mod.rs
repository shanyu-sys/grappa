pub mod post;
pub mod utils;
pub mod video;

use std::{net::SocketAddr, io, time::{SystemTime, Duration, UNIX_EPOCH}, sync::Arc};
use tarpc::{client::{self, Config}, context, tokio_serde::formats::Json};
use tokio::runtime::Runtime;

use post::*;
use crate::{prelude::*, app::socialnet::utils::{VIDEO_STORAGE_SERVER_START, VIDEO_STORAGE_SERVER_NUM}};


pub async fn run(server_addr: [SocketAddr; NUM_SERVERS], server_idx: usize) {
    

    let handle = tokio::spawn(run_server(server_addr[server_idx]));

    let mut guess = String::new();
    println!("Server started, press enter to initialize the application clients");
    io::stdin().read_line(&mut guess).expect("failed to readline");

    dconnect!(server_addr, DCLIENTS, GAMSocialNetClient);

    
    if server_idx >= VIDEO_STORAGE_SERVER_START && server_idx < VIDEO_STORAGE_SERVER_START + VIDEO_STORAGE_SERVER_NUM {
        let video = video::decode().unwrap();
        let mut videos = Vec::with_capacity(4096);
        for _ in 0..4096 {
            videos.push(video.deep_clone());
        }
        let videos_ref = Arc::new(videos);
        unsafe{VIDEOS = Some(videos_ref);}
        println!("video server {} started", server_idx);
    }

    if server_idx == 0 {
        socialnet_benchmark().await;
    }
    
    handle.await.unwrap();
}



