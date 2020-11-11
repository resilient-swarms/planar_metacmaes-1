#ifndef PLANAR_DART_DESCRIPTORS_HPP
#define PLANAR_DART_DESCRIPTORS_HPP

#include <algorithm>
#include <map>
#include <vector>
#include <numeric>

#include <Eigen/Core>
#include <cmath>
#include <cstdlib>
#include <boost/math/constants/constants.hpp>

#include <planar_dart/planar.hpp>


#define DOUBLE_PI boost::math::constants::pi<double>()
#define JOINT_SIZE 8

namespace planar_dart
{

    namespace descriptors
    {
        struct DescriptorBase
        {
        public:
            using robot_t = std::shared_ptr<planar>;
            double factor = 0.5425;    // -0.5425 is the largest y-value in skeleton (link 8)
            double thickness = 0.0775; // thickness of the skeleton
            template <typename Simu, typename robot>
            void operator()(Simu &simu, std::shared_ptr<robot> rob, const Eigen::Vector6d &init_trans)
            {
                assert(false);
            }

            template <typename T>
            void get(T &results)
            {
                assert(false);
            }
        };

        struct PositionalCoord : public DescriptorBase
        {
        public:
            template <typename Simu, typename robot>
            void operator()(Simu &simu, std::shared_ptr<robot> rob, const Eigen::Vector6d &init_trans)
            {
                auto gripper_body = rob->gripper();
                Eigen::Vector3d _posi = gripper_body->getWorldPosition();
                _x = _posi[0];
                _y = _posi[1];
#ifdef GRAPHIC
                std::cout << "gripper position " << _posi << std::endl;
                std::cout << "positional coord " << _x << " , " << _y << std::endl;
#endif
            }

            void get(std::vector<double> &results)
            {
                results.clear();
                //normalise y  such that 1 is furthest from origin and 0 is the origin
                _y = (-_y) / factor; // gripper's y-range in [-factor,0]
                //normalise x  such that 0.5 is origin, 0 and 1 are -factor and factor distance from origirn
                _x = (_x + factor) / (2. * factor); // gripper's x-range in [-factor,factor]
                results.push_back(std::min(std::max(0.0, _x), 1.0));
                results.push_back(std::min(std::max(0.0, _y), 1.0));
            }

        protected:
            double _x, _y;
        };
        struct PolarCoord : public DescriptorBase
        {
        public:
            template <typename Simu, typename robot>
            void operator()(Simu &simu, std::shared_ptr<robot> rob, const Eigen::Vector6d &init_trans)
            {
                auto gripper_body = rob->gripper();
                Eigen::Vector3d _posi = gripper_body->getWorldPosition();
                double _x = _posi[0];
                double _y = _posi[1];
                /* std::string gripper_index = "link_8";
                auto gripper = rob->skeleton()->getBodyNode(gripper_index);
                double _x = gripper->getPosition(0);
                double _y = gripper->getPosition(1);*/
                _d = sqrt(pow(_x, 2) + pow(_y, 2));
                _theta = atan2(_y, _x);
                _theta = _theta <= 0.10 ? _theta + 2. * DOUBLE_PI : _theta;                                                  // 0.10 leaves room for thickness of the robot
                assert((_y > 0) || (_theta <= 2 * DOUBLE_PI + 0.10 && _theta >= DOUBLE_PI - 0.10 && _d <= factor + thickness / 2)); //either illegal move to wall or d in factor and theta [DOUBLE_PI,2DOUBLE_PI]

#ifdef GRAPHIC
                std::cout << "gripper position " << _posi << std::endl;
                std::cout << "polar coord " << _d << " , " << _theta << std::endl;
#endif
            }

            void get(std::vector<double> &results)
            {
                results.clear();
                results.push_back(normalise_radius(_d));
                results.push_back(normalise_angle(_theta));
            }

        protected:
            double _theta, _d;
            double RadMax = 2 * DOUBLE_PI;
            double RadMin = DOUBLE_PI;
            double normalise_radius(double r)
            {
                double temp = r / factor;
                return std::min(std::max(0.0, temp), 1.0);
            }
            double normalise_angle(double angle)
            {
                double temp = (angle - RadMin) / DOUBLE_PI;
                return std::min(std::max(0.0, temp), 1.0);
            }
        };
        struct ResultantAngle : public DescriptorBase
        {
        public:
            template <typename Simu, typename robot>
            void operator()(Simu &simu, std::shared_ptr<robot> rob, const Eigen::Vector6d &init_trans)
            {
                _angles = {};

                Eigen::Vector3d _base(0, 0, 0);

                for (size_t i = 1; i < JOINT_SIZE;)
                {
                    //std::string gripper_index = "joint_" + std::to_string(i+1);
                    //auto gripper_body = rob->skeleton()->getBodyNode(gripper_index)->createMarker();
                    auto joint_body = rob->joint(i);
                    Eigen::Vector3d _posi = joint_body->getWorldPosition();
                    //auto gripper_body = rob->gripper();
                    //Eigen::Vector3d _posi = gripper_body->getWorldPosition();
                    double a = this->getRAngle(_base[0], _base[1], _posi[0], _posi[1]);

                    _angles.push_back(this->normalise(a));

#if GRAPHIC
                    std::cout << "joint " << i << " position " << _posi.transpose() << std::endl;
                    std::cout << "angle " << i << ": " << a << std::endl;
#endif
                    _base = _posi;
                    i += 2;
                }
            }

            void get(std::vector<double> &results)
            {
                results.clear();
                results = _angles;
            }

        protected:
            std::vector<double> _angles;
            double clip_angle(double angle)
            {
                if (angle < -0.10)
                {
                    angle += 2.0 * DOUBLE_PI;
                }
                else if (angle > 2.0 * DOUBLE_PI + 0.10)
                {
                    angle -= 2.0 * DOUBLE_PI;
                }
                else
                {
                    //do nothing
                }
                return angle;
            }
            virtual double normalise(double angle)
            {
                //double MIN = 0;
                // full range possible [0,2Pi] in the absolute coordinate frame
                angle = angle / (2.0 * DOUBLE_PI);
                return std::min(std::max(0., angle), 1.);
            }
            virtual double getRAngle(double P1X, double P1Y, double P2X, double P2Y)
            {

                //relative angle to previous point (in our absolute coordinate frame)
                //note: offset angle not used here
                double dx = P2X - P1X;
                double dy = P2Y - P1Y;
                return clip_angle(atan2(dy, dx));
            }
        };

        struct RelativeResultantAngle : public ResultantAngle
        {

            template <typename Simu, typename robot>
            void operator()(Simu &simu, std::shared_ptr<robot> rob, const Eigen::Vector6d &init_trans)
            {
                _angles = {};

                Eigen::Vector3d _base(0, 0, 0);
                _offset_angle = 1.5*DOUBLE_PI;
                for (size_t i = 1; i < JOINT_SIZE;)
                {
                    //std::string gripper_index = "joint_" + std::to_string(i+1);
                    //auto gripper_body = rob->skeleton()->getBodyNode(gripper_index)->createMarker();
                    auto joint_body = rob->joint(i);
                    Eigen::Vector3d _posi = joint_body->getWorldPosition();
                    //auto gripper_body = rob->gripper();
                    //Eigen::Vector3d _posi = gripper_body->getWorldPosition();
                    double a = this->getRAngle(_base[0], _base[1], _posi[0], _posi[1]);

                    _angles.push_back(this->normalise(a));

#if GRAPHIC
                    std::cout << "joint " << i << " position " << _posi.transpose() << std::endl;
                    std::cout << "angle " << i << ": " << a << std::endl;
                    std::cout << "offset angle" << i << ": " << _offset_angle << std::endl;
#endif
                    _offset_angle = ResultantAngle::getRAngle(_base[0], _base[1], _posi[0], _posi[1]);
                    _base = _posi;
                    i += 2;
                }
            }

        protected:
            double _offset_angle;
            virtual double normalise(double angle)
            {
                // assume downward orientation as in our application (so in [Pi,2Pi] for single segment)
                double RadMax = 0.75 * DOUBLE_PI;  // adding two equal-sized segments with Pi/2 orientation,, coord (-1,-1) --> Pi/4
                double RadMin = -0.75 * DOUBLE_PI; // adding two equal-sized segments with -Pi/2 orientation, coord (1,-1) on cirle wirh r=sqrt(2) --> -Pi/4
                angle = (angle - RadMin) / (RadMax - RadMin);
                return std::min(std::max(0., angle), 1.);
            }
            virtual double getRAngle(double P1X, double P1Y, double P2X, double P2Y)
            {
                //relative angle to previous point (in our *relative* coordinate frame)
                //note: offset angle *is* used here
                double absolute_angle = ResultantAngle::getRAngle(P1X, P1Y, P2X, P2Y);
                return clip_relative_angle(absolute_angle - _offset_angle);
            }
            double clip_relative_angle(double angle)
            {
                if (angle < -0.75 * DOUBLE_PI -0.10)
                {
                    angle += 2.0 * DOUBLE_PI;
                }
                else if (angle > +0.75 * DOUBLE_PI +0.10)
                {
                    angle -= 2.0 * DOUBLE_PI;
                }
                else
                {
                    //do nothing
                }
                return angle;
            }
        };

        struct AngleSum : public DescriptorBase
        {
        public:
            template <typename Simu, typename robot>
            void operator()(Simu &simu, std::shared_ptr<robot> rob, const Eigen::Vector6d &init_trans)
            {
                _sum_angles = {};
                std::vector<double> commands = simu.controller().parameters();
                //Eigen::VectorXd commands = rob->skeleton()->getCommands();
                for (size_t i = 0; i < 6; ++i)
                {
                    double t = commands[i] + commands[i + 1] + commands[i + 2];
                    //double t = (toNormalise(commands[i]) + toNormalise(commands[i+1]) + toNormalise(commands[i+2]))/3.0;
                    _sum_angles.push_back(t / 3.0);
                }
            }

            void get(std::vector<double> &results)
            {
                results.clear();
                results = _sum_angles;
            }

        protected:
            std::vector<double> _sum_angles;
            /*double toNormalise(double angle){
                double MAX = (DOUBLE_PI/2.0);
                double MIN = 0.0 - (DOUBLE_PI/2.0);

                return (angle - MIN)/(MAX - MIN);
            }*/
        };
    } // namespace descriptors
} // namespace planar_dart

#endif
