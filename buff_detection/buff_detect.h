/****************************************************************************
 *  Copyright (C) 2019 cz.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 ***************************************************************************/
#pragma once
#include "iostream"
#include "../common/thread_control.h"
#include "../common/solve_angle/solve_angle.h"
#include "../base.h"
#include "../mainwindow.h"

using namespace cv;
using namespace std;
#define BUFF_DETECT_DEBUG
#ifdef BUFF_DETECT_DEBUG
// ---- buff debug ----
#define DEBUG_DRAW_CONTOURS
#define DEBUG_PUT_TEST_TARGET
#define DEBUG_PUT_TEST_ANGLE
#define DEBUG_DRAW_TARGET
//#define TEST_OTSU
#define AREA_LENGTH_ANGLE 1 // 1:area 2:length 3:diff_angle
//#define FIT

// ---- buff debug ----
#endif

#define DEFAULT 0
#define FIRE 3
#define RESET 4


typedef enum{UNKOWN,INACTION,ACTION}ObjectType;
typedef enum{restore_center=6,follow=-1,shoot=3}mode_buff;

static int target_size=0;
/**
 * @brief 矩形类物体属性
 * 在逻辑识别部分需要修改原有旋转矩形属性
 * 在计算0-360角度上需要用到旋转矩形原始参数
 */
class Object
{
public:
    Object(){}

    RotatedRect fitEllipse_rect;
    RotatedRect minArea_rect;
    RotatedRect small_rect_;
    RotatedRect big_rect_;
    vector<Point2f> points_2d_;
    float angle_;
    Point2f test_point_;
    int direction_ = 1; // 1shun -1ni 0stop

    float length_scale_ = 3;
    float width_scale_ = -3;
    void DrawTarget(Mat &img)
    {
        if(type_ == INACTION)
            circle(img, small_rect_.center, 3, Scalar(0, 0, 255), -1);
        else if(type_ == ACTION)
            circle(img, small_rect_.center, 3, Scalar(255, 255, 255), -1);
        else
            circle(img, small_rect_.center, 3, Scalar(255, 255, 255), 1);
    }

    void UpdateOrder();
    void Indeed_smallrect(RotatedRect minArearect,RotatedRect fitEllipserect);
    void UpdataPredictPoint();

    int type_ = UNKOWN;
};

class AutoAttack
{
public:
    AutoAttack(){}
    int run(bool find_target_flag, float angle_x, float angle_y, int target_size, float gimbal, int move_static);
    int adjust_control(bool find_target_flag, int move_static,int target_size);
private:
    int control_=restore_center;
    int buff_mode;
    int t_tocul=0; //0=little; 1=big
    int restore_count=0;
    int center_buff=0;
    int count_center=0;
};


class FireTask{
public:
    FireTask(){}
    // 开火条件是当输出的角度趋近于０一段时间开火
    int run(Point2f current_angle)
    {
        // 获得发射机会的条件
        Point2f d_angle = Point2f(fabs(last_angle_.x - current_angle.x)
                                  , fabs(last_angle_.y - current_angle.y));
        if(d_angle.x > limit_chance_angle || d_angle.y > limit_chance_angle)
        {
            filter_chance_cnt++;
            if(filter_chance_cnt == limit_filter_chance)
            {
                if(shoot_chance_ == false){
                    printf("获得一次开火机会\r\n");
                    get_change_time = getTickCount();
                }
//                double t = getTickCount();
//                if((t-get_change_time)*1000/getTickFrequency() > 100)
//                {
                    shoot_chance_ = true;
                    filter_chance_cnt = 0;
//                    get_change_time = t;
//                }

            }
        }else{
            filter_chance_cnt = 0;
            last_angle_ = current_angle;
        }

        // 控制发射条件
        int command = DEFAULT;
        if(fabs(current_angle.x) < limit_angle_x_
                && fabs(current_angle.y) < limit_anlge_y_)
        {
            // 满足小于一段时间计数
            cnt_ ++;
        }else {
            // 不满足条件加速减时间
            cnt_ -= 3;
            if(cnt_<0) cnt_ = 0;
        }
        if(cnt_ > max_cnt_)
        {
            cnt_ = 0;
            if(shoot_chance_ == true)
            {
#ifndef NO_FIRE
                command = FIRE;
#else
                command = DEFAULT;
#endif
                fire_cnt++;
                printf("开火:%d\r\n", fire_cnt);
                shoot_chance_ = 0;
            }else {
                command = DEFAULT;
            }
        }else{
            command = DEFAULT;
        }
        return command;
    }
private:
    // 获得发射机会的参数
    Point2f last_angle_;
    int filter_chance_cnt = 0;
    int limit_filter_chance = 5;
    int limit_chance_angle = 4.0;
    double get_change_time;
    bool shoot_chance_ = true;

    // 控制开火的参数
    int cnt_ = 0;
    int fire_cnt = 0;
    int max_cnt_ = 100;             // 满足条件次数
    float limit_angle_x_ = 1.0f;    // 条件角度阈值
    float limit_anlge_y_ = 1.0f;
};

class ResetTask{
public:
    ResetTask(){}
    // 复位归中条件是，丢失目标超过几帧
    int run(bool find_flag)
    {
        int command = DEFAULT;
        if(find_flag == false){
            cnt++;
        }else{
            cnt = 0;
        }
        // 判断归中条件
        if(cnt > max_cnt_)
        {
            command = RESET;
        }else{
            command = DEFAULT;
        }
        return command;
    }
private:
    int cnt = 0;
    int max_cnt_ = 30;   // 最大丢失目标次数
};


class AutoControl
{
public:
    AutoControl(){}
    int run(float current_yaw, float &current_pit,int find_flag){
        int command_tmp = DEFAULT;
        command_tmp = fire_task.run(Point2f(current_yaw, current_pit));
        if(command_tmp != DEFAULT){
            set_fire(command_);
            fire_cnt++;
            //            printf("fire:%d\r\n", fire_cnt);
            //            INFO(command_);
            return command_;
        }
        command_tmp = reset_task.run(find_flag);
        if(command_tmp != DEFAULT){
            // 复位绝对角度
            set_reset(command_);
            current_pit = -5;
            return command_;
        }
        // 通过上面开火任务及复位任务还是得到默认指令，则判断跟随还是不跟随
        if(command_tmp == DEFAULT)
        {
            if(find_flag == 1)
            {
                set_follow(command_);
            }else
            {
                set_no_follow(command_);
            }
        }
        return command_;
    }

private:
    // 设置命令相关函数
    // 置1用 |    清零用&   跟随位0x01 开火位0x02 复位位0x04
    void set_follow(int &command){
        command &= ~0x04;
        command |= 0x01;

    }
    void set_no_follow(int &command){
        command &= ~0x04;
        command &= ~0x01;

    }
    void set_fire(int &command){
        command |= 0x01;
        command ^= 0x02;
    }

    void set_reset(int &command){
        command |= 0x04;
    }

public:
    FireTask fire_task;
    ResetTask reset_task;
    int command_ = 0;
    int fire_cnt = 0;
};


double calcDistanceFor2Point(Point2f p1, Point2f p2);

/**
 * @brief BuffDetectTask 能量机关识别总任务，每帧调用
 * @param img 摄像头获取的RGB图像
 * @return 1是发现目标，0是未发现目标
 */
class BuffDetector
{
public:
    BuffDetector(){
        solve_angle_long_ = SolveAngle(CAMERA1_FILEPATH, LONG_X, LONG_Y, LONG_Z, PTZ_TO_BARREL);
    }
    ~BuffDetector(){}
    void DebugPlotInit(MainWindow *w){
        w_ = w;
    }

    int BuffDetectTask(Mat& img, OtherParam param);
    void getAngle(float &yaw, float &pitch){
        yaw = angle_x_;
        pitch = angle_y_;
    }

    /**
     * @brief 辨别能量机关旋转方向
     * 根据每次识别得到的角度进行滤波，判断能量机关旋转方向
     */
    float getDistance(){
        return distance_;
    }


    int getDirection(float angle);

private:
    bool DetectBuff(Mat& img, OtherParam other_param);
    void setFilter(size_t buff_size, float filter_angle_threshold){
        history_size_=buff_size;
        max_filter_value_=filter_angle_threshold;
    }
    void trigger(int target_size,int move_static);
    int GetIndex(Mat &img, Object object, vector<Rect> all_rect);

private:
    int color_;
    float gimbal;
public:
    int buff_offset_x_ = 69;// id:2 130;
    int buff_offset_y_ = 100;// id:2 135;
    int world_offset_x_ = 500;
    int world_offset_y_ = 500;
    int color_th_ = 30;
    int gray_th_ = 50;
    float buff_angle_ = 0;
    AutoAttack attack;
    int target_size;
    int move_static;
    int do_you_find_inaction=0;
private:
    SolveAngle solve_angle_long_;
    AutoAttack auto_attack;
    AutoControl auto_control;
    MainWindow *w_;
private:
    float angle_x_ = 0;
    float angle_y_ = 0;
    float distance_ = 0;
    vector<Point2f> points_2d;
    int action_cnt_ = 0;

private:
    vector<float> history_;
    size_t history_size_ = 10;
    float last_angle_ = 0;
    float max_filter_value_ = 15;
    int direction_tmp=0;

    int command = 0;
};

float Point_distance(Point2f p1,Point2f p2);



