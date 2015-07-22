#ifndef FBLIB_MULTIVIEW_HOMOGRAPHY_PARAMETERIZATION_H_
#define FBLIB_MULTIVIEW_HOMOGRAPHY_PARAMETERIZATION_H_

#include "fblib/math/numeric.h"

namespace fblib {
	namespace multiview{
		/**   
		 * \brief  �Ե�Ӧ������в�����������Ӧ���� H ��һ����ξ��� ���� 8 �����ɶ�  
		 *  ��Ӧ8������(a, b,...g, h)
		 *  ��Ӧ��Ӧ�������£�
		 *         |a b c|
		 *     H = |d e f|
		 *         |g h 1|
		 */
		template<typename T = double>
		class Homography2DNormalizedParameterization {
		public:
			typedef Eigen::Matrix<T, 8, 1> Parameters;     // a, b, ... g, h
			typedef Eigen::Matrix<T, 3, 3> Parameterized;  // H

			/**	��8����ת�ɵ�Ӧ����
			 */
			static void To(const Parameters &p, Parameterized *h) {
				*h << p(0), p(1), p(2),
					p(3), p(4), p(5),
					p(6), p(7), 1.0;
			}

			/**	����Ӧ����ת8����
			 */
			static void From(const Parameterized &h, Parameters *p) {
				*p << h(0, 0), h(0, 1), h(0, 2),
					h(1, 0), h(1, 1), h(1, 2),
					h(2, 0), h(2, 1);
			}
		};

		/** \brief  �Ե�Ӧ������в���������3D ��Ӧ���� H ��һ����ξ��� ���� 15 �����ɶ�
		 *  ��Ӧ15������ (a, b,...n, o)
		 *  ��Ӧ��Ӧ�������£�
		 *          |a b c d|
		 *      H = |e f g h|
		 *          |i j k l|
		 *          |m n o 1|
		 */
		template<typename T = double>
		class Homography3DNormalizedParameterization {
		public:
			typedef Eigen::Matrix<T, 15, 1> Parameters;     // a, b, ... n, o
			typedef Eigen::Matrix<T, 4, 4>  Parameterized;  // H

			/**	��15����ת�ɵ�Ӧ����
			*/
			static void To(const Parameters &p, Parameterized *h) {
				*h << p(0), p(1), p(2), p(3),
					p(4), p(5), p(6), p(7),
					p(8), p(9), p(10), p(11),
					p(12), p(13), p(14), 1.0;
			}

			/**	����Ӧ����ת15����
			*/
			static void From(const Parameterized &h, Parameters *p) {
				*p << h(0, 0), h(0, 1), h(0, 2), h(0, 3),
					h(1, 0), h(1, 1), h(1, 2), h(1, 3),
					h(2, 0), h(2, 1), h(2, 2), h(2, 3),
					h(3, 0), h(3, 1), h(3, 2);
			}
		};
	} //namespace multiview
}  // namespace fblib

#endif  // FBLIB_MULTIVIEW_HOMOGRAPHY_PARAMETERIZATION_H_
