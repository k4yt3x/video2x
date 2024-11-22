# History

Video2X came a long way from its original concepts to what it has become today. It started as a simple concept of "waifu2x can upscale images, and a video is just a sequence of images". Then, a PoC was made which can barely upscale a single video with waifu2x-caffe and with fixed settings. Now, Video2X has become a comprehensive and customizable video upscaling tool with a nice GUI and a community around it. This article documents in detail how Video2X's concept was born, and what happened during its development.

## Origin

The story started with me watching Bad Apple!!'s PV in early 2017. The original PV has a size of `512x384`, which is quite small and thus, quite blurry.

![vlcsnap-2020-05-15-20h41m36s060](https://user-images.githubusercontent.com/21986859/82106016-08ba8800-970e-11ea-85b5-d1b57d34e283.png)\
_A screenshot of the original Bad Apple!! PV_

Around the same time, I was introduced to this amazing project named waifu2x, which upscales (mostly anime) images using machine learning. This created a spark in my head: **if images can be upscaled, aren't videos just a sequence of images?** Then, I started making a proof-of-concept by manually extracting all frames from the original PV using FFmpeg, putting them through waifu2x-caffe, and assembling the frames back into a video again using FFmpeg. This was how the ["4K BadApple!! waifu2x Lossless Upscaled"](https://www.youtube.com/watch?v=FiX7ygnbAHw) video was created.

![4K BadApple waifu2x](https://img.youtube.com/vi/FiX7ygnbAHw/maxresdefault.jpg)\
_Thumbnail of the "4K BadApple!! waifu2x Lossless Upscaled" video_

After this experiment completed successfully, I started thinking about making an automation pipeline, where this manual process will be streamlined, and each of the steps will be handled automatically.

## Proof-of-Concept

When I signed up for Hack the Valley II in late 2017, I didn't know what I was going to make during that hackathon. Our team sat down and thought about what to make for around an hour, but no one came up with anything interesting. All of a sudden, I remembered, "Hey, isn't there a PoC I wanted to make? How about making that our hackathon project?" I then temporarily name the project Video2X, following waifu2x's scheme. Video2X was then born.

I originally wanted to write Video2X for Linux, but it's too complicated to get the original [nagadomi/waifu2x](https://github.com/nagadomi/waifu2x)'s version of waifu2x running, so waifu2x-caffe written for Windows was used to save time. This is why the first version of Video2X only supports Windows, and can only use waifu2x-caffe as its upscaling driver.

![the first commit of Video2X](https://user-images.githubusercontent.com/21986859/82105271-fcccc700-9709-11ea-8861-b22b807f885f.png)\
_video2x.py file in the first version of Video2X_

At the end of the hackathon, we managed to make a [sample comparison video](https://www.youtube.com/watch?v=mGEfasQl2Zo) based on [Spirited Away's official trailer](https://www.youtube.com/watch?v=ByXuk9QqQkk). This video was then published on YouTube and is the same demo video showcased in Video2X's repository. The original link was at [https://www.youtube.com/watch?v=PG94iPoeoZk](https://www.youtube.com/watch?v=PG94iPoeoZk), but it has been moved lately to another account under K4YT3X's name.

![Spirited Away Demo](https://user-images.githubusercontent.com/21986859/49412428-65083280-f73a-11e8-8237-bb34158a545e.png)\
_Upscale Comparison Demonstration_

When we demoed this project, there wasn't so much interest expressed by the judges. We were, however, suggested to pitch our project to Adobe. That didn't end up going anywhere, either. Like most of the other projects in a hackathon, this project didn't win any awards, and just almost vanished after the hackathon was over.

<!--![Hack the Valley II](https://files.k4yt3x.com/Resources/Images/htv2_team_photo.png)\-->

_[Image Removed]_\
_Our team in Hack the Valley II. You can see Video2X's demo video on the computer screens. Image blurred for privacy._

## Video2X 2.0

Roughly three months after the hackathon, I came back to this project and decided it was worth continuing. Although not many people in the hackathon found this project interesting or useful, I saw value in this project. This was further reinforced by the stars I've received in the project's repository.

I continued working on enhancing Video2X and fixing bugs, and Video2X 2.0 was released. The original version of Video2X was only made as a proof-of-concept for the hackathon. A lot of the usability and convenience aspects are ignored in exchange for development speed. The 2.0 version addressed a lot of these issues and made Video2X usable for regular users. Video2X has then also been converted from a hackathon project to a personal open-source project.

![screenshot of Video2X 2.0](https://user-images.githubusercontent.com/21986859/40265170-39c0caae-5b01-11e8-8371-8b6c24769639.png)\
_Screenshot of Video2X 2.0_
