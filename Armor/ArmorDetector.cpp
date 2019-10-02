﻿#include "ArmorDetector.h"

ArmorDetector::ArmorDetector()
    : state(State::SEARCHING_STATE)
{
    // TODO: 从串口数据加载颜色
    enemy_color = EnemyColor::BLUE;

    // TODO: 从文件读取参数
    enable_debug = true;

    color_thresh = 230;
    blue_thresh = 50;
    red_thresh = 50;

    light_min_area = 50;

    armor_max_angle_diff = 15;
    armor_max_aspect_ratio = 6;
    armor_max_height_ratio = 2;
    armor_min_area = 200;
}

ArmorDetector::~ArmorDetector()
{
}

void ArmorDetector::DetectArmor(cv::Mat& src, cv::Point3f& target_3d)
{
    switch (state) {
    case ArmorDetector::State::SEARCHING_STATE:
        SearchArmor(src);
        break;
    case ArmorDetector::State::TRACKING_STATE:
        // 跟踪
        break;
    default:
        break;
    }
}

void ArmorDetector::SearchArmor(const cv::Mat& src)
{
    if (enable_debug) {
        detect_lights = src.clone();
        armors_befor_filter = src.clone();
        armors_after_filter = src.clone();
    }
    double start = static_cast<double>(cv::getTickCount());

    std::vector<cv::RotatedRect> lights;
    DetectLights(src, lights);

    std::vector<ArmorInfo> armors;
    PossibleArmors(lights, armors);

    double time = ((double)cv::getTickCount() - start) / cv::getTickFrequency();
    std::cout << "\ntime: " << time;
}

void ArmorDetector::DetectLights(const cv::Mat& src, std::vector<cv::RotatedRect>& lights)
{
    cv::Mat light = DistillationColor(src);

    cv::Mat gray_image;
    cv::cvtColor(src, gray_image, cv::COLOR_BGR2GRAY);

    cv::Mat binary_brightness_image;
    cv::threshold(gray_image, binary_brightness_image, color_thresh, 255, cv::THRESH_BINARY);

    cv::Mat binary_color_image;
    float thresh = (enemy_color == EnemyColor::BLUE) ? blue_thresh : red_thresh;
    cv::threshold(light, binary_color_image, thresh, 255, cv::THRESH_BINARY);

    if (enable_debug) {
        cv::imshow("binary_brightness_image", binary_brightness_image);
        cv::imshow("binary_color_img", binary_color_image);
    }

    std::vector<std::vector<cv::Point>> contours_color;
    cv::findContours(binary_color_image, contours_color, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<std::vector<cv::Point>> contours_brightness;
    cv::findContours(binary_brightness_image, contours_brightness, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    lights.reserve(contours_color.size());
    for (unsigned int i = 0; i < contours_brightness.size(); ++i) {
        for (unsigned int j = 0; j < contours_color.size(); ++j) {
            if (cv::pointPolygonTest(contours_color[j], contours_brightness[i][0], false) >= 0.0) {
                cv::RotatedRect single_light = cv::minAreaRect(contours_brightness[i]);
                /* Filter Lights */
                cv::Rect bounding_rect = single_light.boundingRect();
                if (bounding_rect.height < bounding_rect.width || single_light.size.area() < light_min_area)
                    break;
                lights.push_back(single_light);
                if (enable_debug)
                    DrawRotatedRect(detect_lights, single_light, cv::Scalar(0, 255, 0), 2);
                break;
            }
        }
    }
    if (enable_debug) {
        cv::imshow("lights_before_filter", detect_lights);
        /*
        auto c = cv::waitKey(1);
        if (c == 'a')
            cv::waitKey(0);
        */
    }
}

cv::Mat ArmorDetector::DistillationColor(const cv::Mat& src)
{
    std::vector<cv::Mat> bgr;
    cv::split(src, bgr);
    cv::Mat result;
    if (EnemyColor::BLUE == enemy_color)
        cv::subtract(bgr[0], bgr[2], result);
    else
        cv::subtract(bgr[2], bgr[0], result);
    return result;
}

void ArmorDetector::DrawRotatedRect(const cv::Mat& image, const cv::RotatedRect& rect, const cv::Scalar& color, int thickness)
{
    /* put Text on image
    std::ostringstream oss;
    int rect_angle = (rect.angle < -45) ? rect.angle + 90 : rect.angle;
    oss << rect_angle;
    cv::String text(oss.str());
    cv::putText(image, text, rect.center, cv::FONT_HERSHEY_COMPLEX, 0.5, cv::Scalar(0, 100, 0), thickness, cv::LINE_AA);
    */
    cv::Point2f vertices[4];
    rect.points(vertices);
    for (int i = 0; i < 4; i++)
        cv::line(image, vertices[i], vertices[(i + 1) % 4], color, thickness, cv::LINE_AA);
}

void ArmorDetector::PossibleArmors(const std::vector<cv::RotatedRect>& lights, std::vector<ArmorInfo>& armors)
{
    for (unsigned int i = 0; i < lights.size(); ++i) {
        for (unsigned int j = i + 1; j < lights.size(); ++j) {
            cv::RotatedRect light1 = lights[i];
            cv::RotatedRect light2 = lights[j];
            auto edge1 = std::minmax(light1.size.width, light1.size.height);
            auto edge2 = std::minmax(light2.size.width, light2.size.height);
            float light1_angle = (light1.angle <= -45.0) ? light1.angle + 90 : light1.angle;
            float light2_angle = (light2.angle <= -45.0) ? light2.angle + 90 : light2.angle;
            float angle_diff = std::abs(light1_angle - light2_angle);
            double center_angle = (double)std::atan((lights[i].center.y - lights[j].center.y) / (lights[i].center.x - lights[j].center.x)) * 180 / CV_PI;
            cv::RotatedRect rect;
            rect.angle = static_cast<float>(center_angle);
            rect.center = (light1.center + light2.center) / 2;
            rect.size.width = std::sqrt((lights[i].center.x - lights[j].center.x) * (lights[i].center.x - lights[j].center.x) + (lights[i].center.y - lights[j].center.y) * (lights[i].center.y - lights[j].center.y));
            rect.size.height = std::max<float>(edge1.second, edge2.second);
            auto height = std::minmax(edge1.second, edge2.second);
            if (enable_debug) {
                std::cout << "\ncenter_angle = " << center_angle;
                std::cout << "\nlight1_angle = " << light1_angle;
                std::cout << "\nlight2_angle = " << light2_angle;
                std::cout << "\nangle_diff = " << angle_diff;
                std::cout << "\nrect_angle = " << rect.angle;
                std::cout << "\nrect_width = " << rect.size.width;
                std::cout << "\nrect_height = " << rect.size.height;
                std::cout << "\nwidth/height = " << rect.size.width / rect.size.height;
                std::cout << "\nedge1.first = " << edge1.first;
                std::cout << "\nedge2.first = " << edge2.first;
                std::cout << "\nedge1.second = " << edge1.second;
                std::cout << "\nedge2.second = " << edge2.second;
                std::cout << "\nheight1/height2 = " << height.second / height.first << '\n';
                std::cout << "\nrect.area = " << rect.size.area();
            }
            if (angle_diff < armor_max_angle_diff
                && std::max(abs(center_angle - light1_angle), abs(center_angle - light2_angle)) < armor_max_angle_diff
                && (rect.size.width / rect.size.height) < armor_max_aspect_ratio
                && (height.second / height.first) < armor_max_height_ratio
                && rect.size.area() > armor_min_area) {
                if (enable_debug)
                    DrawRotatedRect(armors_befor_filter, rect, cv::Scalar(0, 255, 0), 2);
                /*
				std::vector<cv::Point2f> armor_points;
                if (light1.center.x < light2.center.x)
                    CalcArmorInfo(armor_points, light1, light2);
                else
                    CalcArmorInfo(armor_points, light2, light1);
                */
            }
        }
    }
    if (enable_debug) {
        cv::imshow("armors_before_filter", armors_befor_filter);
        auto c = cv::waitKey(1);
        if (c == 'a')
            cv::waitKey(0);
    }
}