#include <uhd/types/tune_request.hpp>
#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/transport/udp_simple.hpp>
#include <uhd/exception.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <complex>
#include <fftw3.h>

uhd::usrp::multi_usrp::sptr usrp;

std::string addr("127.0.0.1");
std::string device_args("addr=192.168.10.6");
std::string subdev("A:0");
std::string ant("TX/RX");
std::string ref("internal");
std::string port("7124"); //UDP Port

double rate(1e6);
double freq(1955e6);
double gain(20);
double bw(20e6);
size_t total_num_samps(200e6);

void setupUSRP();

int UHD_SAFE_MAIN(int argc, char *argv[]) {
	uhd::set_thread_priority_safe();
	setupUSRP();

	//Check Ref and LO Lock detect
	std::vector<std::string> sensor_names;
	sensor_names = usrp->get_rx_sensor_names(0);
	if (std::find(sensor_names.begin(), sensor_names.end(), "lo_locked") != sensor_names.end()) {
		uhd::sensor_value_t lo_locked = usrp->get_rx_sensor("lo_locked", 0);
		std::cout << boost::format("Checking RX: %s ...") % lo_locked.to_pp_string() << std::endl;
		UHD_ASSERT_THROW(lo_locked.to_bool());
	}

	//create fftw_plan for later
	fftwf_plan p;
	fftwf_complex *in, *out;
	
	//create a receive streamer
	uhd::stream_args_t stream_args("fc32", "sc16"); //complex floats
	uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);

	//setup streaming
	uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
	stream_cmd.num_samps = total_num_samps;
	stream_cmd.stream_now = true;
	rx_stream->issue_stream_cmd(stream_cmd);

	//loop until total number of samples reached
	size_t num_acc_samps = 0; //number of accumulated samples
	uhd::rx_metadata_t md;
	std::vector<std::complex<float> > buff(rx_stream->get_max_num_samps());
	uhd::transport::udp_simple::sptr udp_xport = uhd::transport::udp_simple::make_connected(addr, port);

	size_t buffer_size = rx_stream->get_max_num_samps();
	in = (fftwf_complex*)fftwf_alloc_complex(rx_stream->get_max_num_samps());
	out = in;

	p = fftwf_plan_dft_1d(buffer_size, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

	while (num_acc_samps < total_num_samps) {
		size_t num_rx_samps = rx_stream->recv(
			in, buffer_size, md
		);

		//handle the error codes
		switch (md.error_code) {
			case uhd::rx_metadata_t::ERROR_CODE_NONE:
				break;

			case uhd::rx_metadata_t::ERROR_CODE_TIMEOUT:
				if (num_acc_samps == 0) continue;
				std::cout << boost::format(
					"Got timeout before all samples received, possible packet loss, exiting loop..."
				) << std::endl;
				goto done_loop;

			default:
				std::cout << boost::format(
					"Got error code 0x%x, exiting loop..."
				) % md.error_code << std::endl;
				goto done_loop;
		}

		fftwf_execute(p);

		std::vector<float> output(buffer_size);
		// Convert to power spectrum, which is the magnitude of each frequency component squared.
		/*for (int i = 0; i < buffer_size; i++)
		{
			output.at(i) = std::sqrtf(powf(in[i][0], 2), powf(in[i][1], 2));	// take magnitude
		}
		*/

		/*
		for (std::vector<std::complex<float> >::iterator it = buff.begin(); it != buff.end(); it++) {
			*it = std::norm(*it);
		}*/

		//send complex single precision floating point samples over udpstd::
		udp_xport->send(boost::asio::buffer(in, num_rx_samps * sizeof(fftwf_complex)));

		num_acc_samps += num_rx_samps;
	} done_loop:

	//cleanup fftw
	fftwf_destroy_plan(p);
	fftwf_free(in); fftwf_free(out);


	//finished
	std::cout << std::endl << "Done!" << std::endl << std::endl;

	return EXIT_SUCCESS;
}

void setupUSRP() {
	//create a usrp device
	std::cout << std::endl;
	std::cout << boost::format("Creating the usrp device with: %s...") % device_args << std::endl;
	usrp = uhd::usrp::multi_usrp::make(device_args);
	std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;

	//Lock mboard clocks
	usrp->set_clock_source(ref);

	//always select the subdevice first, the channel mapping affects the other settings
	usrp->set_rx_subdev_spec(subdev);
	std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;

	//set the rx sample rate
	std::cout << boost::format("Setting RX Rate: %f Msps...") % (rate / 1e6) << std::endl;
	usrp->set_rx_rate(rate);
	std::cout << boost::format("Actual RX Rate: %f Msps...") % (usrp->get_rx_rate() / 1e6) << std::endl << std::endl;

	//set the rx center frequency
	std::cout << boost::format("Setting RX Freq: %f MHz...") % (freq / 1e6) << std::endl;
	uhd::tune_request_t tune_request(freq);
	tune_request.args = uhd::device_addr_t("mode_n=integer");
	usrp->set_rx_freq(tune_request);
	std::cout << boost::format("Actual RX Freq: %f MHz...") % (usrp->get_rx_freq() / 1e6) << std::endl << std::endl;

	//set the rx rf gain
	std::cout << boost::format("Setting RX Gain: %f dB...") % gain << std::endl;
	usrp->set_rx_gain(gain);
	std::cout << boost::format("Actual RX Gain: %f dB...") % usrp->get_rx_gain() << std::endl << std::endl;

	//set the analog frontend filter bandwidth
	std::cout << boost::format("Setting RX Bandwidth: %f MHz...") % (bw / 1e6) << std::endl;
	usrp->set_rx_bandwidth(bw);
	std::cout << boost::format("Actual RX Bandwidth: %f MHz...") % (usrp->get_rx_bandwidth() / 1e6) << std::endl << std::endl;

	//set the antenna
	usrp->set_rx_antenna(ant);

	boost::this_thread::sleep(boost::posix_time::seconds(1)); //allow for some setup time
}