#pragma once
class option
{
public:
	option();
	~option();
	bool parse(int argc, char *argv[]);
	void print_usage(const char *command = nullptr);

	const char *input_text_path, *output_text_path;
	const char *tweet_buffer_path, *tweet_id_path, *word_path, *user_path, *summary_path;
	const char *input_param_path_prefix, *output_param_path_prefix;
	const char *input_tweet_param_path, *output_tweet_param_path;
	const char *input_user_param_path, *output_user_param_path;
	const char *input_topic_param_path, *output_topic_param_path;
	const char *hyper_param_path;

	const char *command;
	size_t thread_num;
	size_t batch_size;
	int iteration_num;

	double alpha_m1, beta_m1, beta_bg_m1, gamma_m1;
	int topic_num;

	const char *stopword_path;
	int min_user_freq, min_word_freq;
};

