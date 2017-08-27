/*!
  @file   FaceTracker.cpp
  @author David Hirvonen
  @brief  Declaration of a face landmark tracker.

  \copyright Copyright 2017 Elucideye, Inc. All rights reserved.
  \license{This project is released under the 3 Clause BSD License.}

*/

#include "drishti/face/FaceTracker.h" // FaceModel.h
#include "drishti/core/hungarian.h" 
#include "drishti/core/make_unique.h"

DRISHTI_FACE_NAMESPACE_BEGIN

struct FaceTracker::Impl
{
    using FaceTrack = std::pair<drishti::face::FaceModel, FaceTracker::TrackInfo>;
    using FaceTrackVec = std::vector<FaceTrack>;    
    using FaceModelVec = std::vector<drishti::face::FaceModel>;    
    
    Impl(float costThreshold, std::size_t minTrackHits, std::size_t maxTrackMisses)
        : m_costThreshold(costThreshold)
        , m_minTrackHits(minTrackHits)
        , m_maxTrackMisses(maxTrackMisses)
    {
    }

    void update(const FaceModelVec &facesIn, FaceTrackVec &facesOut)
    {
        if(m_tracks.size() == 0)
        {
            // Initialize tracks:
            for (const auto &f : facesIn)
            {
                m_tracks.emplace_back(f, TrackInfo(m_id++));
            }
        }
        else
        {
            
            // assign new detections to existing tracks:
            core::DMatchVec outMatches;
            core::IntVec inliers1, inliers2;
            if(facesIn.size())
            {
                // ... linear assignment
                cv::Mat1f C(m_tracks.size(), facesIn.size(), std::numeric_limits<float>::max());
                for(int i = 0; i < m_tracks.size(); i++)
                {
                    for(int j = 0; j < facesIn.size(); j++)
                    {
                        C(i,j) = cv::norm(*m_tracks[i].first.eyesCenter - *facesIn[j].eyesCenter);
                    }
                }
                core::hungarian(C, outMatches, inliers1, inliers2, C.rows, C.cols);
            }

            std::vector<std::uint8_t> hits(m_tracks.size(), 0);
            for(const auto &m : outMatches)
            {
                // Create a new track, or update an old track:
                if(m.distance > m_costThreshold)
                {
                    m_tracks.emplace_back(facesIn[m.trainIdx], TrackInfo(m_id++));
                }
                else
                {
                    m_tracks[m.queryIdx].second.hit();
                    m_tracks[m.queryIdx].first = facesIn[m.trainIdx];
                    hits[m.queryIdx] = 1;
                }
            }
            
            for(int i = 0; i < hits.size(); i++)
            {
                if(!hits[i])
                {
                    m_tracks[i].second.miss();
                }
            }
            
            // Prune old tracks:
            m_tracks.erase(std::remove_if(m_tracks.begin(), m_tracks.end(), [this](const FaceTrack &track) {
               return (track.second.misses >= m_maxTrackMisses);
            }), m_tracks.end());
        }
        
        // Return subset of remaining mature tracks:
        std::copy_if(m_tracks.begin(), m_tracks.end(), std::back_inserter(facesOut), [this](const FaceTrack &track) {
           return (track.second.age > m_minTrackHits);
        });
    }

    float m_costThreshold = 0.15; // meters
    std::size_t m_minTrackHits = 3;
    std::size_t m_maxTrackMisses = 3;
    
    std::size_t m_id = 0;
    
    FaceTrackVec m_tracks;
};

FaceTracker::FaceTracker(float costThreshold, std::size_t minTrackHits, std::size_t maxTrackMisses)
{
    m_impl = drishti::core::make_unique<Impl>(costThreshold, minTrackHits, maxTrackMisses);
}

FaceTracker::~FaceTracker() = default;

void FaceTracker::operator()(const FaceModelVec &facesIn, FaceTrackVec &facesOut)
{
    m_impl->update(facesIn, facesOut);
}

DRISHTI_FACE_NAMESPACE_END
