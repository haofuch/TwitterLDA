#pragma once

#include "file_reader.h"
#include "utility.h"
#include "parallel.h"
#include <unordered_set>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <random>

class model : public parallel
{
public:
	enum infer_mode
	{
		probability, score
	};

	model(const char *summary_path, int topic_num, double alpha_m1, double beta_m1, double beta_bg_m1, double gamma_m1, size_t thread_num);
	model(int topic_num, int word_num, double alpha_m1, double beta_m1, double beta_bg_m1, double gamma_m1, size_t thread_num);
	model(const char *hyper_param_path, size_t thread_num);
	~model();

	void init_param(const char *tweet_path, const char *user_param_path, const char *tweet_param_path, unsigned int rand_seed = 5489);
	void load_hyper_param(const char *path);
	void save_hyper_param(const char *path);
	void load_topic_param(const char *path);
	void save_topic_param(const char *path);
	double iterate(const char *tweet_path, size_t batch_size, const char *input_user_path, const char *input_tweet_path, const char *output_user_path, const char *output_tweet_path);

	int infer(std::vector<int> &words, infer_mode mode, double *probs = nullptr);

	void save_user_topic_distribution(const char *user_param_path, const char *output_path);
	void save_topic_word_distribution(const char *output_path);

	void save_user_topic_distribution_text(const char *user_param_path, const char *user_path, const char *output_path);
	void save_topic_word_distribution_text(const char *word_path, const char *output_path);
	void save_tweet_topic_text(const char *tweet_param_path, const char *tweet_path, const char *tweet_id_path, const char *output_path);

	double topic_word_density();

	int topic_num() const;
	int word_num() const;

	static void make_buffer(const char *input_path, const char *buffer_path, const char *user_path, const char *word_path, const char *tweet_id_path, const char *summary_path, const char *stopword_path = nullptr, int min_user_freq = 0, int min_word_freq = 0);

private:
	int _topic_num;
	int _word_num;
	
	int **_topic_word_counts;
	long long *_total_word_counts;
	long long *_topic_all_word_counts;

	double _alpha_m1, _beta_m1, _beta_bg_m1, _gamma_m1;

	utility::read_buffer *_tweet_read_buffers;
	utility::write_buffer *_tweet_param_write_buffers;
	utility::read_buffer *_tweet_param_read_buffers;
	std::default_random_engine *_random_engines;

	std::unordered_map<int, int> _user_indexes;
	std::vector<int*> _user_topic_counts;
	std::vector<int> _user_all_topic_counts;
	std::vector<int> _user_ids;

	void _init();
	void _update(size_t id);

	void _sample(utility::read_buffer &tweet_read_buffer, utility::read_buffer &tweet_param_read_buffer, utility::write_buffer &tweet_param_write_buffer, std::default_random_engine &random_engine);
	inline void _inc_topic_word_count(int topic, int word);
	inline void _dec_topic_word_count(int topic, int word);
};

