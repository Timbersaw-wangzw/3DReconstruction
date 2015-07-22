/*************************************************************************
 * �ļ��� test.cpp
 * ʱ�䣺 2015/04/29 15:09
 * ���ߣ� ���
 * �ʼ��� fengbing123@gmail.com
 *
 * ˵���� exif����
 *************************************************************************/
#include <memory>
#include "fblib/image/exif_simple.h"
#include "fblib/utils/cmd_line.h"

using namespace fblib::utils;
int main(int argc, char *argv[])
{
	CmdLine cmd;
	std::string input_image;
	cmd.add(make_option('i', input_image, "imagefile"));
	try{
		if (argc == 1) throw std::string("Invalid command line parameter.");
		cmd.process(argc, argv);
	}
	catch (const std::string &s){
		std::cerr << "Usage: " << argv[0] << ' '
			<< "[-i|--imagefile path] "
			<< std::endl;

		std::cerr << s << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << "You called: " << std::endl
		<< argv[0] << std::endl
		<< "--imagefile " << input_image << std::endl;

	fblib::image::EXIFSimple exif_sample;
	exif_sample.open(input_image);

	std::cout << "width : " << exif_sample.getWidth() << std::endl;
	std::cout << "height : " << exif_sample.getHeight() << std::endl;
	std::cout << "focal : " << exif_sample.getFocalLength() << std::endl;
	std::cout << "brand : " << exif_sample.getBrand() << std::endl;
	std::cout << "model : " << exif_sample.getModel() << std::endl;
	getchar();
	return EXIT_SUCCESS;
}