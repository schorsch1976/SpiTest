#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <cstdint>
#include <regex>
#include <string>
#include <vector>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

struct Params
{
	bool verbose{false};

	int mode{0};
	std::string device;
	uint32_t speed{0};
	int bytes_to_read{0};
	int delay_us{0};

	std::string wbytes;
};

void Unescape(std::string &data)
{
	// split string in items
	std::vector<std::string> split;
	size_t pos = 0;
	while (1)
	{
		size_t next = data.find(' ', pos);
		if (next == std::string::npos)
		{
			if (pos < data.size())
			{
				split.emplace_back(std::string(data.begin() + pos, data.end()));
			}
			break;
		}

		split.emplace_back(
			std::string(data.begin() + pos, data.begin() + next));
		pos = next + 1;
	}

	// convert items to bytes
	std::string result;
	result.reserve(split.size());

	std::regex ex_hex("0x([0-9a-fA-F]{1,2})");
	std::regex ex_dez("[0-9]{1,3}");

	for (auto &item : split)
	{
		std::smatch match;
		if (std::regex_match(item, match, ex_hex))
		{
			uint16_t tmp{0};
			std::istringstream iss{match[0].str()};
			iss >> std::hex >> tmp;
			result.push_back(static_cast<char>(tmp));
		}
		else if (std::regex_match(item, match, ex_dez))
		{
			uint16_t tmp{0};
			std::istringstream iss{match[0].str()};
			iss >> tmp;
			result.push_back(static_cast<char>(tmp));
		}
		else
		{
			std::copy(item.begin(), item.end(), std::back_inserter(result));
		}
	}

	data = result;
}

void Print(const std::string &prefix, const std::string &data)
{
	std::cout << prefix;
	for (auto &c : data)
	{
		std::cout << " 0x" << std::hex << std::setfill('0') << std::setw(2)
				  << static_cast<uint16_t>(static_cast<uint8_t>(c));
	}
	std::cout << '\n';
}

#ifdef UNIX
// spidev
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct Hdl
{
	explicit Hdl(int fd) : m_fd{fd} {}
	~Hdl()
	{
		if (m_fd >= 0)
		{
			close(m_fd);
			m_fd = -1;
		}
	}

	operator int() { return m_fd; }

	int m_fd{-1};
};

int DoWork(const Params &params)
{
	Hdl fd(open(params.device.c_str(), O_RDWR));
	if (fd < 0)
	{
		perror("open");
		return EXIT_FAILURE;
	}

	// set the spi mode
	char mode = params.mode & 0x03; // take the lower 2 bits
	int result = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if (result == -1)
	{
		perror("ioctl: SPI_IOC_WR_MODE");
		return EXIT_FAILURE;
	}

	result = ioctl(fd, SPI_IOC_RD_MODE, &mode);
	if (result == -1)
	{
		perror("ioctl: SPI_IOC_RD_MODE");
		return EXIT_FAILURE;
	}

	// speed
	result = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &params.speed);
	if (result == -1)
	{
		perror("ioctl: SPI_IOC_WR_MAX_SPEED_HZ");
		return EXIT_FAILURE;
	}
	result = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &params.speed);
	if (result == -1)
	{
		perror("ioctl: SPI_IOC_RD_MAX_SPEED_HZ");
		return EXIT_FAILURE;
	}

	// bits per bits_per_word
	uint8_t wordsize = 8;
	result = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &wordsize);
	if (result == -1)
	{
		perror("ioctl: SPI_IOC_RD_BITS_PER_WORD");
		return EXIT_FAILURE;
	}
	result = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &wordsize);
	if (result == -1)
	{
		perror("ioctl: SPI_IOC_WR_BITS_PER_WORD");
		return EXIT_FAILURE;
	}

	// Now do the write and read
	int number_of_requests = 0;
	number_of_requests += params.wbytes.size() > 0 ? 1 : 0;
	number_of_requests += params.bytes_to_read > 0 ? 1 : 0;

	int index = 0;
	std::array<spi_ioc_transfer, 2> requests;
	for (auto &c : requests)
	{
		std::memset(&c, 0, sizeof(spi_ioc_transfer));
	}
	if (params.wbytes.size() > 0)
	{
		requests[index].tx_buf = reinterpret_cast<__u64>(params.wbytes.data());
		requests[index].len = params.wbytes.size();
		requests[index].delay_usecs = params.delay_us;
		index++;
	}

	std::string rbuffer;
	rbuffer.resize(params.bytes_to_read);
	if (params.bytes_to_read > 0)
	{
		requests[index].rx_buf = reinterpret_cast<__u64>(rbuffer.data());
		requests[index].len = rbuffer.size();
		requests[index].delay_usecs = params.delay_us;
	}

	if (number_of_requests > 0)
	{
		if (params.verbose)
		{
			Print("TX:", params.wbytes);
		}

		int result =
			ioctl(fd, SPI_IOC_MESSAGE(number_of_requests), &requests[0]);
		if (result < -1)
		{
			perror("ioctl: SPI_IOC_MESSAGE");
			return EXIT_FAILURE;
		}
		Print("RX:", rbuffer);
	}
	return EXIT_SUCCESS;
}

#else
int DoWork(const Params &params)
{
	Print("TX:", params.wbytes);
	return EXIT_SUCCESS;
}
#endif

int main(int argc, char **argv)
{
	try
	{
		// Declare the supported options.
		po::options_description desc("Available options");

		Params params{};

		std::string write;
		// clang-format off
		desc.add_options()
			("help,h",			"produce help message")
			("verbose,v",		"be verbose and print everything")

			("mode,m",			po::value<int>(&params.mode)->default_value(0),			
								"Spi Mode: 0-3")

			("device",			po::value<std::string>(&params.device)->default_value("/dev/spidev0.0"),
								"device")

			("speed,s",			po::value<uint32_t>(&params.speed)->default_value(100000),
								"Bits per second")

			("write,w",			po::value<std::string>(&params.wbytes)->default_value(""),
								"Write these bytes to spi. '0xaa' will be one byte. '123 134' will be two bytes. Other ascii chars will be just one byte per character")

			("bytes_to_read,r",	po::value<int>(&params.bytes_to_read)->default_value(1),
								"Read this number of bytes from the SPI after the write")

			("delay_us,d",		po::value<int>(&params.delay_us)->default_value(100),		
								"delay in microseconds between read and write")
			;
		// clang-format on

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);

		if (vm.count("help") || vm.empty())
		{
			std::cout << "Usage: spitest [OPTION]\n" << '\n' << desc << '\n';
			return EXIT_FAILURE;
		}

		params.verbose = vm.count("verbose");

		Unescape(params.wbytes);
		return DoWork(params);
	}
	catch (const boost::program_options::error &ex)
	{
		std::cerr << "Program options error: " << ex.what() << std::endl;
	}
	catch (const std::exception &ex)
	{
		std::cerr << "Error: " << ex.what() << std::endl;
	}
	catch (...)
	{
		std::cerr << "Unknown error" << std::endl;
	}
	return EXIT_FAILURE;
}