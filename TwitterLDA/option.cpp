#include "option.h"
#include "utility.h"
#include <cstdio>
#include <cstring>

static const char *option_names[][2] =
{
	{ "thread", "Number of threads (default 1)" },
	{ "batch", "Batch size in megabyte (default 16)" },
	{ "iterate", "Number of iterations (default 100)" },
	{ "input", "Input tweet text file" },
	{ "output", "Output text file" },
	{ "hyper-param", "Hyperparameter file" },
	{ "alpha-m1", "Alpha minus one (default 0.5)" },
	{ "beta-m1", "Beta minus one (default 0.01)" },
	{ "beta-bg-m1", "Background beta minus one (default 0.1)" },
	{ "gamma-m1", "Gamma minus one (default 20.0)" },
	{ "topic", "Number of topics (default 100)" },
	{ "buffer", "Path prefix of buffer files" },
	{ "input-param", "Path prefix of input parameter files" },
	{ "output-param", "Path prefix of output parameter file" },
	{ "stopword", "Stopwords list file" },
	{ "user-freq", "Minimum user frequency (default 1)" },
	{ "word-freq", "Minimum word frequency (default 1)" },
	{ nullptr, nullptr }
};

static const char *command_names[][4] =
{
	{ "make-buffer", "Convert text corpus to binary buffer", "[stopword] [user-freq] [word-freq] input", "buffer" },
	{ "train", "Train the model", "[thread] [batch] [iterate] [alpha-m1] [beta-m1] [beta-bg-m1] [gamma-m1] [topic] buffer", "output-param hyper-param" },
	{ "train-cont", "Continue training the model", "[thread] [batch] [iterate] input-param buffer hyper-param", "output-param" },
	{ "infer-prob", "Infer top topic (in terms of probability) from text file", "[thread] [batch] input buffer hyper-param input-param", "output" },
	{ "infer-score", "Infer top topic (in terms of score) from text file", "[thread] [batch] input buffer hyper-param input-param", "output" },
	{ "dump-topic", "Dump topic-word distribution to text file", "buffer hyper-param input-param", "output" },
	{ "dump-user", "Dump user-topic distribution to text file", "buffer hyper-param input-param", "output" },
	{ "dump-tweet", "Dump topic of tweet to text file", "input buffer hyper-param input-param", "output" },
	{ nullptr, nullptr, nullptr, nullptr }
};

static void delete_string(const char *ptr)
{
	if (ptr != nullptr) delete[] ptr;
}

option::option()
{
	input_text_path = output_text_path = nullptr;
	tweet_buffer_path = tweet_id_path = word_path = user_path = summary_path = nullptr;
	input_param_path_prefix = output_param_path_prefix = nullptr;
	input_tweet_param_path = output_tweet_param_path = nullptr;
	input_user_param_path = output_user_param_path = nullptr;
	input_topic_param_path = output_topic_param_path = nullptr;
	hyper_param_path = nullptr;
	command = nullptr;
	stopword_path = nullptr;

	min_word_freq = 1;
	min_user_freq = 1;

	thread_num = 1;
	batch_size = 16 << 20;
	iteration_num = 100;

	alpha_m1 = 0.5;
	beta_m1 = 0.01;
	beta_bg_m1 = 0.1;
	gamma_m1 = 20.0;
	
	topic_num = 100;
}

option::~option()
{
	delete_string(input_text_path);
	delete_string(output_text_path);
	delete_string(tweet_buffer_path);
	delete_string(tweet_id_path);
	delete_string(word_path);
	delete_string(user_path);
	delete_string(summary_path);
	delete_string(input_param_path_prefix);
	delete_string(output_param_path_prefix);
	delete_string(input_tweet_param_path);
	delete_string(output_tweet_param_path);
	delete_string(input_user_param_path);
	delete_string(output_user_param_path);
	delete_string(input_topic_param_path);
	delete_string(output_topic_param_path);
	delete_string(hyper_param_path);
	delete_string(command);
	delete_string(stopword_path);
}

bool option::parse(int argc, char *argv[])
{
	if (argc < 2) return false;
	command = utility::new_string(argv[1]);
	bool is_valid_command = false;
	for (int i = 0; command_names[i][0] != nullptr; ++i)
	{
		if (strcmp(command_names[i][0], command) == 0)
		{
			is_valid_command = true;
			break;
		}
	}
	if (!is_valid_command)
	{
		printf("Invalid command %s\n", command);
		return false;
	}
	if (argc < 3)
	{
		printf("No option specified\n");
		return false;
	}

	for (int i = 2; i < argc; i += 2)
	{
		char *option_name = argv[i];
		if (strlen(option_name) < 3 || option_name[0] != '-' || option_name[1] != '-')
		{
			printf("Invalid option %s\n", option_name);
			return false;
		}
		char *option_value = argv[i + 1];
		
		if (strcmp(option_name + 2, "thread") == 0)
		{
			thread_num = atoi(option_value);
		}
		else if (strcmp(option_name + 2, "batch") == 0)
		{
			batch_size = atoi(option_value) << 20;
		}
		else if (strcmp(option_name + 2, "iterate") == 0)
		{
			iteration_num = atoi(option_value);
		}
		else if (strcmp(option_name + 2, "input") == 0)
		{
			input_text_path = utility::new_string(option_value);
		}
		else if (strcmp(option_name + 2, "output") == 0)
		{
			output_text_path = utility::new_string(option_value);
		}
		else if (strcmp(option_name + 2, "hyper-param") == 0)
		{
			hyper_param_path = utility::new_string(option_value);
		}
		else if (strcmp(option_name + 2, "alpha-m1") == 0)
		{
			alpha_m1 = atof(option_value);
		}
		else if (strcmp(option_name + 2, "beta-m1") == 0)
		{
			beta_m1 = atof(option_value);
		}
		else if (strcmp(option_name + 2, "beta-bg-m1") == 0)
		{
			beta_bg_m1 = atof(option_value);
		}
		else if (strcmp(option_name + 2, "gamma-m1") == 0)
		{
			gamma_m1 = atof(option_value);
		}
		else if (strcmp(option_name + 2, "topic") == 0)
		{
			topic_num = atoi(option_value);
		}
		else if (strcmp(option_name + 2, "buffer") == 0)
		{
			tweet_buffer_path = utility::new_string(option_value, ".buffer.bin");
			tweet_id_path = utility::new_string(option_value, ".id.bin");
			word_path = utility::new_string(option_value, ".word.txt");
			user_path = utility::new_string(option_value, ".user.txt");
			summary_path = utility::new_string(option_value, ".summary.txt");
		}
		else if (strcmp(option_name + 2, "input-param") == 0)
		{
			input_param_path_prefix = utility::new_string(option_value);
			input_tweet_param_path = utility::new_string(option_value, ".tweet-param.bin");
			input_user_param_path = utility::new_string(option_value, ".user-param.bin");
			input_topic_param_path = utility::new_string(option_value, ".topic-param.bin");
		}
		else if (strcmp(option_name + 2, "output-param") == 0)
		{
			output_param_path_prefix = utility::new_string(option_value);
			output_tweet_param_path = utility::new_string(option_value, ".tweet-param.bin");
			output_user_param_path = utility::new_string(option_value, ".user-param.bin");
			output_topic_param_path = utility::new_string(option_value, ".topic-param.bin");
		}
		else if (strcmp(option_name + 2, "stopword") == 0)
		{
			stopword_path = utility::new_string(option_value);
		}
		else if (strcmp(option_name + 2, "user-freq") == 0)
		{
			min_user_freq = atoi(option_value);
		}
		else if (strcmp(option_name + 2, "word-freq") == 0)
		{
			min_word_freq = atoi(option_value);
		}
		else
		{
			printf("Invalid option %s\n", option_name);
			return false;
		}
	}

	bool is_option_missing = false;
	for (int i = 0; command_names[i][0] != nullptr; ++i)
	{
		if (strcmp(command_names[i][0], command) != 0) continue;

		for (int j = 2; j < 4; ++j)
		{
			const char *ptr = command_names[i][j];
			while (true)
			{
				const char *next = strchr(ptr, ' ');
				size_t len = (next == nullptr) ? strlen(ptr) : next - ptr;
				char *name = new char[len + 1];
				memcpy(name, ptr, len);
				name[len] = '\0';
				if (ptr[0] != '[')
				{
					bool found = false;
					for (int k = 2; k < argc; k += 2)
					{
						if (strcmp(name, argv[k] + 2) == 0)
						{
							found = true;
							break;
						}
					}
					if (!found)
					{
						printf("Missing option --%s\n", name);
						is_option_missing = true;
					}
				}
				delete[] name;
				if (next == nullptr) break;
				ptr += len + 1;
			}
		}
	}
	if (is_option_missing) return false;

	return true;
}

void option::print_usage(const char *command)
{
	if (command == nullptr)
	{
		printf("Usage: TwitterLDA <command> <options>\n");
		printf("Commands:\n");
		for (int i = 0; command_names[i][0] != nullptr; ++i)
		{
			printf("  %-14s %s\n", command_names[i][0], command_names[i][1]);
		}
		printf("Options:\n");
		for (int i = 0; option_names[i][0] != nullptr; ++i)
		{
			printf("  --%-12s %s\n", option_names[i][0], option_names[i][1]);
		}
		printf("Text file format:\n");
		printf("  user <tab> word <space> word <space> word ..., or\n");
		printf("  word <space> word <space> word ...\n");
	}
	else
	{
		for (int i = 0; command_names[i][0] != nullptr; ++i)
		{
			if (strcmp(command_names[i][0], command) == 0)
			{
				printf("Command %s: %s\n", command_names[i][0], command_names[i][1]);
				printf("Input:  %s\n", command_names[i][2]);
				printf("Output: %s\n", command_names[i][3]);
				return;
			}
		}
	}
}
