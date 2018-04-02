/*
背景减除 Background subtraction (BS)
背景减除在很多基础应用中占据很重要的角色。
列如顾客统计，使用一个静态的摄像头来记录进入和离开房间的人数，
或者交通摄像头，需要提取交通工具的信息等。
我们需要把单独的人或者交通工具从背景中提取出来。
技术上说，我们需要从静止的背景中提取移动的前景.

提供一个无干扰的背景图像
实时计算当前图像和 背景图像的差异（图像做差）  阈值二值化 得到 多出来的物体(mask)   再区域分割

BackgroundSubtractorMOG2 是以高斯混合模型为基础的背景/前景分割算法。
它是以2004年和2006年Z.Zivkovic的两篇文章为基础。
这个算法的一个特点是它为每个像素选择一个合适的高斯分布。
这个方法有一个参数detectShadows，默认为True，他会检测并将影子标记出来，
但是这样做会降低处理速度。影子会被标记成灰色。


 背景与前景都是相对的概念，以高速公路为例：有时我们对高速公路上来来往往的汽车感兴趣，
这时汽车是前景，而路面以及周围的环境是背景；有时我们仅仅对闯入高速公路的行人感兴趣，
这时闯入者是前景，而包括汽车之类的其他东西又成了背景。背景剪除是使用非常广泛的摄像头视频中探测移动的物体。
这种在不同的帧中检测移动的物体叫做背景模型，其实背景剪除也是前景检测。



一个强劲的背景剪除算法应当能够解决光强的变化，杂波的重复性运动，和长期场景的变动。
下面的分析中会是用函数V(x,y,t)表示视频流，t是时间，x和y代表像素点位置。
例如，V(1,2,3)是在t=3时刻，像素点(1,2)的光强。下面介绍几种背景剪除的方法。


【1】 利用帧的不同（临帧差）
	该方法假定是前景是会动的，而背景是不会动的，而两个帧的不同可以用下面的公式：
        	D(t+1) = I(x,y,t+1) - I(x,y,t)
	用它来表示同个位置前后不同时刻的光强只差。
	只要把那些D是0的点取出来，就是我们的前景，同时也完成了背景剪除。
	当然，这里的可以稍作改进，不一定说背景是一定不会动的。
	可以用一个阀值来限定。看下面的公式：
          	I(x,y,t+1) - I(x,y,t) > 阈值
 通过Th这个阀值来进行限定，把大于Th的点给去掉，留下的就是我们想要的前景。

    #define threshold_diff 10 临帧差阈值
    // 可使用 矩阵相减 
    subtract(gray1, gray2, sub);  

	for (int i = 0;i<bac.rows; i++)  
	    for (int j = 0;j<bac.cols; j++)  
		if (abs(bac.at<unsigned char>(i, j)) >= threshold_diff)
                    //这里模板参数一定要用 unsigned char 8位(灰度图)，否则就一直报错  
		    bac.at<unsigned char>(i, j) = 255;  
		else bac.at<unsigned char>(i, j) = 0;
 


【2】 均值滤波（Mean filter） 当前帧 和 过去帧均值  做差

      M(x,y,t) = 1/N SUM(I(x,y,(1..t)))
      I(x,y,t+1) - M(x,y,t) > 阈值        为前景

	不是通过当前帧和上一帧的差别，而是当前帧和过去一段时间的平均差别来进行比较，
	同时通过阀值Th来进行控制。当大于阀值的点去掉，留下的就是我们要的前景了。

【3】 使用高斯均值  + 马尔科夫过程
// 初始化：
     均值 u0 = I0 均值为第一帧图像
     方差 c0 = (默认值)
// 迭代：
       ut = m * It + (1-m) * ut-1       马尔科夫过程
       d  = |It - ut|                   计算差值
       ct = m * d^2  + (1-m) * ct-1^2   更新 方差

判断矩阵 d/ct > 阈值    为前景


*/
  
//opencv
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/video.hpp>
//C
#include <stdio.h>
//C++
#include <iostream>
#include <sstream>
using namespace cv;
using namespace std;

// 全局变量
Mat frame;      //当前帧
Mat fgMaskMOG2; //mask 掩码 fg mask fg mask generated by MOG2 method
Ptr<BackgroundSubtractor> pMOG2; //背景去除算法 MOG2 Background subtractor
int keyboard; //键盘按键 值 input from keyboard

void help();
void processVideo(char* videoFilename);
void processImages(char* firstFrameFilename);

//帮助信息
void help()
{
    cout
    << "--------------------------------------------------------------------------" << endl
    << "This program shows how to use background subtraction methods provided by "  << endl
    << " OpenCV. You can process both videos (-vid) and images (-img)."             << endl
                                                                                    << endl
    << "Usage:"                                                                     << endl
    << "./bs {-vid <video filename>|-img <image filename>}"                         << endl
    << "for example: ./bs -vid video.avi"                                           << endl
    << "or: ./bs -img /data/images/1.png"                                           << endl
    << "--------------------------------------------------------------------------" << endl
    << endl;
}


int main(int argc, char* argv[])
{
    // 打印帮助信息
    help();
    // 检查命令行参数
    if(argc != 3) {
        cerr <<"Incorret input list" << endl;
        cerr <<"exiting..." << endl;
        return EXIT_FAILURE;
    }
    //创建窗口 GUI windows
    namedWindow("Frame");//当前帧
    namedWindow("FG Mask MOG 2");
    //创建背景去除对象 
    pMOG2 = createBackgroundSubtractorMOG2(); //MOG2 approach
// pMOG2 =  createBackgroundSubtractorKNN(); 的方法也能检测出移动的物体 
    if(strcmp(argv[1], "-vid") == 0) {
        //视频
        processVideo(argv[2]);//处理视频
    }
    else if(strcmp(argv[1], "-img") == 0) {
        //图片集
        processImages(argv[2]);//处理图像
    }
    else {
        //error in reading input parameters
        cerr <<"Please, check the input parameters." << endl;
        cerr <<"Exiting..." << endl;
        return EXIT_FAILURE;
    }
    //destroy GUI windows
    destroyAllWindows();
    return EXIT_SUCCESS;
}

void processVideo(char* videoFilename) {
    //视频 捕获对象
    VideoCapture capture(videoFilename);
    if(!capture.isOpened()){
        cerr << "Unable to open video file: " << videoFilename << endl;
        exit(EXIT_FAILURE);
    }

    //读取输入视频  按 ESC 、 'q'键 退出 
    while( (char)keyboard != 'q' && (char)keyboard != 27 ){
        //读取当前帧 图像
        if(!capture.read(frame)) {
            cerr << "Unable to read next frame." << endl;
            cerr << "Exiting..." << endl;
            exit(EXIT_FAILURE);
        }
        //更新背景
        pMOG2->apply(frame, fgMaskMOG2);//得到 mask
        //get the frame number and write it on the current frame
        stringstream ss;//字符串 流
        rectangle(frame, cv::Point(10, 2), cv::Point(100,20),
                  cv::Scalar(255,255,255), -1);
        ss << capture.get(CAP_PROP_POS_FRAMES);//当前帧名字 字符串流
        string frameNumberString = ss.str();// 字符串流 -> 字符串
	// 显示帧名字 
        putText(frame, frameNumberString.c_str(), cv::Point(15, 15),
                FONT_HERSHEY_SIMPLEX, 0.5 , cv::Scalar(0,0,0));
        // 显示当前帧 和 背景去除后的mask show the current frame and the fg masks
        imshow("Frame", frame);
        imshow("FG Mask MOG 2", fgMaskMOG2);
//  形态学开运算去噪点
// fgmask = cv2.morphologyEx(fgmask, cv2.MORPH_OPEN, kernel)
// #寻找视频中的轮廓
//im, contours, hierarchy = cv2.findContours(fgmask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
//for c in contours:
//       #计算各轮廓的周长
//      perimeter = cv2.arcLength(c,True)
//       if perimeter > 188:
//           #找到一个直矩形（不会旋转）
//            x,y,w,h = cv2.boundingRect(c)
//          #画出这个矩形
//         cv2.rectangle(frame,(x,y),(x+w,y+h),(0,255,0),2)    


        //get the input from the keyboard
        keyboard = waitKey( 30 );
    }
    // 释放 视频捕捉对象
    capture.release();
}

// 图像集 处理
void processImages(char* fistFrameFilename) {
    //读取第一张图像
    frame = imread(fistFrameFilename);
    if(frame.empty()){
        cerr << "Unable to open first image frame: " << fistFrameFilename << endl;
        exit(EXIT_FAILURE);
    }
    //当前帧名
    string fn(fistFrameFilename);
    //读取输入图片  按 ESC 、 'q'键 退出 
    while( (char)keyboard != 'q' && (char)keyboard != 27 ){
        //更新背景
        pMOG2->apply(frame, fgMaskMOG2);
        //get the frame number and write it on the current frame
        size_t index = fn.find_last_of("/");
        if(index == string::npos) {
            index = fn.find_last_of("\\");
        }
        size_t index2 = fn.find_last_of(".");
        string prefix = fn.substr(0,index+1);
        string suffix = fn.substr(index2);
        string frameNumberString = fn.substr(index+1, index2-index-1);

        istringstream iss(frameNumberString);

        int frameNumber = 0;
        iss >> frameNumber;

        rectangle(frame, cv::Point(10, 2), cv::Point(100,20),
                  cv::Scalar(255,255,255), -1);

        putText(frame, frameNumberString.c_str(), cv::Point(15, 15),
                FONT_HERSHEY_SIMPLEX, 0.5 , cv::Scalar(0,0,0));

        //显示当前帧 和mask show the current frame and the fg masks
        imshow("Frame", frame);
        imshow("FG Mask MOG 2", fgMaskMOG2);

        // 读取按键
        keyboard = waitKey( 30 );

        //search for the next image in the sequence
        ostringstream oss;
        oss << (frameNumber + 1);
        string nextFrameNumberString = oss.str();
        string nextFrameFilename = prefix + nextFrameNumberString + suffix;
        //read the next frame
        frame = imread(nextFrameFilename);
        if(frame.empty()){
            //error in opening the next image in the sequence
            cerr << "Unable to open image frame: " << nextFrameFilename << endl;
            exit(EXIT_FAILURE);
        }
        //update the path of the current frame
        fn.assign(nextFrameFilename);
    }
}

