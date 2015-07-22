#ifndef FBLIB_TRACKING_REGION_TRACKER_H_
#define FBLIB_TRACKING_REGION_TRACKER_H_

#include "fblib/image/image.h"
using namespace fblib::image;

namespace fblib {
	namespace tracking{
		
		/** \brief	������ٵĻ��� */
		class RegionTracker {
		public:
			RegionTracker() {}
			virtual ~RegionTracker() {}

			/**
			 * \brief   ����һ�����\a image1 �� \a image2
			 * 			\a x2, \a y2 Ϊ\a image2 �в²�λ�ã����û�в²⣬����ʼ��Ϊ(\a x1, \a y1)
			 *
			 * \param	image1	  	��һ��ͼ��
			 * \param	image2	  	�ڶ���ͼ��
			 * \param	x1		  	��ʼ������xֵ
			 * \param	y1		  	��ʼ������yֵ
			 * \param [in,out]	x2	������²�ֵ�������Ӧ����ֵ ��Ӧx����
			 * \param [in,out]	y2	������²�ֵ�������Ӧ����ֵ ��Ӧy����
			 *
			 * \return	true if it succeeds, false if it fails.
			 */
			virtual bool Track(const Image<float> &image1,
				const Image<float> &image2,
				double  x1, double  y1,
				double *x2, double *y2) const = 0;
		};
	} //namespace tracking
}  // namespace fblib

#endif  // FBLIB_TRACKING_REGION_TRACKER_H_
