#include <cstdio>
#include <cstdlib>
#include <vector>
#include "file_reader.h"
#include "utility.h"
#include "model.h"
#include "inference.h"
#include "option.h"
#include "parallel.h"

void make_buffer(option &opt)
{
	model::make_buffer(
		opt.input_text_path, 
		opt.tweet_buffer_path, 
		opt.user_path, 
		opt.word_path, 
		opt.tweet_id_path, 
		opt.summary_path, 
		opt.stopword_path, 
		opt.min_user_freq, 
		opt.min_word_freq);
}

void train(option &opt)
{
	model *m;

	char *tweet_param_paths[2] =
	{
		utility::new_string(opt.output_param_path_prefix, ".tweet-param.temp0.bin"),
		utility::new_string(opt.output_param_path_prefix, ".tweet-param.temp1.bin")
	};
	char *user_param_paths[2] =
	{
		utility::new_string(opt.output_param_path_prefix, ".user-param.temp0.bin"),
		utility::new_string(opt.output_param_path_prefix, ".user-param.temp1.bin")
	};

	if (opt.input_param_path_prefix == nullptr)
	{
		m = new model(opt.summary_path, opt.topic_num, opt.alpha_m1, opt.beta_m1, opt.beta_bg_m1, opt.gamma_m1, opt.thread_num);
		m->save_hyper_param(opt.hyper_param_path);
		m->init_param(opt.tweet_buffer_path, user_param_paths[0], tweet_param_paths[0]);
	}
	else
	{
		m = new model(opt.hyper_param_path, opt.thread_num);
		m->load_topic_param(opt.input_topic_param_path);
	}

	for (int iter = 1; iter <= opt.iteration_num; ++iter)
	{
		const char *input_user_param_path, *output_user_param_path;
		const char *input_tweet_param_path, *output_tweet_param_path;
		
		if (iter == 1 && opt.input_param_path_prefix != nullptr)
		{
			input_user_param_path = opt.input_user_param_path;
			input_tweet_param_path = opt.input_tweet_param_path;
		}
		else
		{
			input_user_param_path = user_param_paths[(iter - 1) % 2];
			input_tweet_param_path = tweet_param_paths[(iter - 1) % 2];
		}

		if (iter == opt.iteration_num)
		{
			output_user_param_path = opt.output_user_param_path;
			output_tweet_param_path = opt.output_tweet_param_path;
		}
		else
		{
			output_user_param_path = user_param_paths[iter % 2];
			output_tweet_param_path = tweet_param_paths[iter % 2];
		}

		printf("Iteration %d\n", iter);
		double update_ratio = m->iterate(opt.tweet_buffer_path, opt.batch_size, input_user_param_path, input_tweet_param_path, output_user_param_path, output_tweet_param_path);
	}

	m->save_topic_param(opt.output_topic_param_path);

	delete[] tweet_param_paths[0];
	delete[] tweet_param_paths[1];
	delete[] user_param_paths[0];
	delete[] user_param_paths[1];
	delete m;
}

void dump_topic(option &opt)
{
	model m(opt.hyper_param_path, 0);
	m.load_topic_param(opt.input_topic_param_path);
	m.save_topic_word_distribution_text(opt.word_path, opt.output_text_path);
}

void dump_user(option &opt)
{
	model m(opt.hyper_param_path, 0);
	m.save_user_topic_distribution_text(opt.input_user_param_path, opt.user_path, opt.output_text_path);
}

void dump_tweet(option &opt)
{
	model m(opt.hyper_param_path, 0);
	m.save_tweet_topic_text(opt.input_tweet_param_path, opt.input_text_path, opt.tweet_id_path, opt.output_text_path);
}

void infer_prob(option &opt)
{
	model m(opt.hyper_param_path, 0);
	m.load_topic_param(opt.input_topic_param_path);
	inference infer(m, model::infer_mode::probability, opt.word_path, opt.thread_num);
	infer.infer(opt.input_text_path, opt.batch_size, opt.output_text_path);
}

void infer_score(option &opt)
{
	model m(opt.hyper_param_path, 0);
	m.load_topic_param(opt.input_topic_param_path);
	inference infer(m, model::infer_mode::score, opt.word_path, opt.thread_num);
	infer.infer(opt.input_text_path, opt.batch_size, opt.output_text_path);
}

const void *command_table[][2] =
{
	{ "make-buffer", &make_buffer },
	{ "train", &train },
	{ "train-cont", &train },
	{ "infer-prob", infer_prob },
	{ "infer-score", infer_score },
	{ "dump-topic", &dump_topic },
	{ "dump-user", &dump_user },
	{ "dump-tweet", &dump_tweet },
	{ nullptr, nullptr}
};

int main(int argc, char *argv[])
{
	option opt;
	if (!opt.parse(argc, argv))
	{
		opt.print_usage(opt.command);
		return 1;
	}

	for (int i = 0; command_table[i][0] != nullptr; ++i)
	{
		if (strcmp(opt.command, (const char *)command_table[i][0]) == 0)
		{
			void(*func)(option*);
			func = (void(*)(option*))command_table[i][1];
			func(&opt);
		}
	}

	return 0;
}