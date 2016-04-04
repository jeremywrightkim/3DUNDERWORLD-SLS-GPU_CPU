#include "ReconstructorCPU.h"
#include <cassert>
#include "fileReader.h"
#include <fstream>
namespace SLS{

ReconstructorCPU::~ReconstructorCPU()
{
    for (auto &cam: cameras_)
        delete cam;
    delete projector_;
}
void ReconstructorCPU::initBuckets()
{
    size_t x,y;
    projector_->getSize(x,y);
    buckets_.resize(cameras_.size());
    for (auto& b: buckets_)
        b.resize(1 << projector_->getRequiredNumFrames());
    generateBuckets();
}
void ReconstructorCPU::addCamera(Camera *cam)
{
    cameras_.push_back(cam);
}
void ReconstructorCPU::generateBuckets()
{
    //Generating reconstruction bucket for each camera
    //for each camera
    //for (auto &cam : cameras_)
    for (size_t camIdx = 0; camIdx < cameras_.size(); camIdx++)
    {
        auto &cam = cameras_[camIdx];
        LOG::writeLog("Generating reconstruction bucket for \"%s\" ... \n", cam->getName().c_str());

        //cam->undistort();
        cam->computeShadowsAndThreasholds();

        size_t x=0,y=0,xTimesY=0;
        cam->getResolution(x,y);
        xTimesY=x*y;

        // For each camera pixle
        //assert(dynamic_cast<FileReader*>(cam)->getNumFrames() == projector_->getRequiredNumFrames()*2+2);

        for ( size_t i=0; i<xTimesY; i++)
        {
            if (! cam->queryMask(i) )
                continue;
            cam->nextFrame();cam->nextFrame();//skip first two frames
            size_t bitIdx = 0;

            Dynamic_Bitset bits(projector_->getRequiredNumFrames());

            bool discard=false;

            // for each image
            while(bitIdx < projector_->getRequiredNumFrames())
            {

                auto frame = cam->getNextFrame();
                auto invFrame = cam->getNextFrame();
                auto pixel = frame.at<uchar>(i%y,i/y);
                auto invPixel = invFrame.at<uchar>(i%y,i/y);



                // Not considering shadow mask. But the following test should be
                // more strict than shadow mask.
                if (invPixel > pixel && invPixel-pixel >= cam->getThreashold(i))
                //if (invPixel > pixel && invPixel-pixel >= cam->getWhiteThreshold())
                    bits.clearBit(bitIdx);
                else if (pixel > invPixel && pixel-invPixel > cam->getThreashold(i))
                //else if (pixel > invPixel && pixel-invPixel > cam->getWhiteThreshold())
                    bits.setBit(bitIdx);
                else
                {
                    //discard this pixel
                    discard=true;
                    break;
                }
                bitIdx++;
            }
            if (!discard)
                buckets_[camIdx][bits.to_uint()].push_back(i);
        }
    }

}

void ReconstructorCPU::renconstruct()
{
    /* Assuming there are two cameras 
     * For each projector pixel,
     * find the min dis
     * */
    size_t x,y;
    cameras_[0]->getResolution(x,y);
    initBuckets();

    LOG::startTimer();
    std::ofstream of("test.obj");
    std::ofstream ofAvg("testAvg.obj");

    for ( size_t i=0; i<buckets_[0].size(); i++)
    {
        //Progress
        const auto &cam0bucket = buckets_[0][i];
        const auto &cam1bucket = buckets_[1][i];
        if ((!cam0bucket.empty()) && (!cam1bucket.empty()))
        {
            float minDist=9999999999.0;
            glm::vec4 minMidP(0.0f);

            //auto midP = midPointBkp(
            //        cameras_[0]->getRay(cam0bucket[0]),
            //        cameras_[1]->getRay(cam1bucket[0]),
            //        minDist);
            //if (minDist > 10) continue;
            //of<<"v "<<midP.x<<" "<<midP.y<<" "<<midP.z<<std::endl;

            glm::vec4 pointSum(0.0);
            size_t pointCount=0;

            for (const auto& cam0P: cam0bucket)
                for (const auto& cam1P: cam1bucket)
                {
                    float dist=0.0;
                    
                    auto midP=midPointBkp(cameras_[0]->getRay(cam0P), cameras_[1]->getRay(cam1P), dist);
                    if (dist < -0.5) continue;
                    if (dist < minDist)
                    {
                        minDist = dist;
                        minMidP = midP;
                    }
                    //if (dist < 0.5)
                    {
                        pointSum += midP;
                        pointCount++;
                    }
                }
            if (minDist > 1.0) continue;
            of<<"v "<<minMidP.x<<" "<<minMidP.y<<" "<<minMidP.z<<std::endl;
            if (pointCount != 0)
                ofAvg<<"v "<<pointSum.x / (float) pointCount<<" "<<pointSum.y / (float) pointCount<<" "<<pointSum.z / (float) pointCount<<std::endl;
            unsigned char r, g, b;
        }
    }
    of.close();
    ofAvg.close();
    LOG::endTimer("Finished reconstruction in ");
}


} // namespace SLS
