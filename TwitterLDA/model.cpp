#include "model.h"
#include "file_reader.h"
#include "utility.h"
#include <cstring>
#include <cstdio>
#include <cassert>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <random>
#include <vector>
#include <chrono>

void model::_init()
{
	_topic_word_counts = utility::new_array<int>(_topic_num + 1, _word_num);
	_total_word_counts = new long long[2];
	_topic_all_word_counts = new long long[_topic_num + 1];

	_tweet_read_buffers = new utility::read_buffer[_thread_num];
	_tweet_param_read_buffers = new utility::read_buffer[_thread_num];
	_tweet_param_write_buffers = new utility::write_buffer[_thread_num];
	_random_engines = new std::default_random_engine[_thread_num];
}


model::model(int topic_num, int word_num, double alpha_m1, double beta_m1, double beta_bg_m1, double gamma_m1, size_t thread_num) : parallel(thread_num)
{
	_topic_num = topic_num;
	_word_num = word_num;

	_alpha_m1 = alpha_m1;
	_beta_m1 = beta_m1;
	_beta_bg_m1 = beta_bg_m1;
	_gamma_m1 = gamma_m1;

	_init();
}

model::model(const char *summary_path, int topic_num, double alpha_m1, double beta_m1, double beta_bg_m1, double gamma_m1, size_t thread_num) : parallel(thread_num)
{
	load_hyper_param(summary_path);
	_topic_num = topic_num;

	_alpha_m1 = alpha_m1;
	_beta_m1 = beta_m1;
	_beta_bg_m1 = beta_bg_m1;
	_gamma_m1 = gamma_m1;

	_init();
}

model::model(const char *hyper_param_path, size_t thread_num) : parallel(thread_num)
{
	load_hyper_param(hyper_param_path);
	_init();
}

model::~model()
{
	delete[] _tweet_read_buffers;
	delete[] _tweet_param_read_buffers;
	delete[] _tweet_param_write_buffers;
	delete[] _random_engines;

	delete[] _topic_all_word_counts;
	delete[] _total_word_counts;
	utility::delete_array(_topic_word_counts);

	for (size_t i = 0; i < _user_topic_counts.size(); ++i) delete[] _user_topic_counts[i];
}

int model::topic_num() const
{
	return _topic_num;
}

int model::word_num() const
{
	return _word_num;
}

void model::_update(size_t id)
{
	_sample(_tweet_read_buffers[id], _tweet_param_read_buffers[id], _tweet_param_write_buffers[id], _random_engines[id]);
}

void model::init_param(const char *tweet_path, const char *user_param_path, const char *tweet_param_path, unsigned int rand_seed)
{
	std::default_random_engine random_engine(rand_seed);
	std::uniform_int_distribution<int> topic_distr(0, _topic_num - 1);
	std::uniform_int_distribution<int> word_distr(0, 0xff);

	int *topic_counts = new int[_topic_num];
	std::fill(topic_counts, topic_counts + _topic_num, 0);
	for (int i = 0; i <= _topic_num; ++i)
	{
		std::fill(_topic_word_counts[i], _topic_word_counts[i] + _word_num, 0);
	}
	_total_word_counts[0] = _total_word_counts[1] = 0;
	std::fill(_topic_all_word_counts, _topic_all_word_counts + _topic_num + 1, 0);

	tweet_file_reader tweet_reader(tweet_path);

	utility::write_buffer user_param_buffer, tweet_param_buffer;
	int user = -1;
	FILE *fp_user_param = fopen(user_param_path, "wb");
	FILE *fp_tweet_param = fopen(tweet_param_path, "wb");
	while (true)
	{
		file_item item = tweet_reader.get_item(false);
		utility::read_buffer tweet_buffer(item.data, item.size);
		int curr_user;
		if (item.size == 0 || tweet_buffer.read_varint(&curr_user) == 0 || curr_user != user)
		{
			// got new user, write parameters of the previous user
			if (user >= 0)
			{
				user_param_buffer.clear();
				user_param_buffer.write_varint(user);
				user_param_buffer.write_sparse_array(topic_counts, _topic_num, 0);
				utility::fwrite(user_param_buffer.buffer(), user_param_buffer.size(), fp_user_param);
			}
			if (item.size > 0)
			{
				user = curr_user;
				std::fill(topic_counts, topic_counts + _topic_num, 0);
			}
		}
		if (item.size == 0) break;

		// initialize topic
		int topic = topic_distr(random_engine);
		++topic_counts[topic];
		tweet_param_buffer.clear();
		tweet_param_buffer.write_varint(topic);

		// initialize words
		int word_count;
		tweet_buffer.read_varint(&word_count);
		tweet_param_buffer.write_varint(word_count);

		for (int i = 0; i < word_count; i += 8)
		{
			char value = word_distr(random_engine);
			if (i + 8 > word_count) value &= (1 << (word_count - i)) - 1;
			tweet_param_buffer.write(value);
			for (int j = 0; j < 8 && i + j < word_count; ++j)
			{
				int word;
				tweet_buffer.read_varint(&word);
				if (value & (1 << j))
				{
					_inc_topic_word_count(topic, word);
				}
				else
				{
					_inc_topic_word_count(_topic_num, word);
				}
			}
		}

		utility::fwrite(tweet_param_buffer.buffer(), tweet_param_buffer.size(), fp_tweet_param);
	}
	fclose(fp_user_param);
	fclose(fp_tweet_param);
}

void model::load_hyper_param(const char *path)
{
	text_file_reader reader(path);
	while (true)
	{
		file_item item = reader.get_item(false);
		if (item.size == 0) break;
		char *ptr = strchr(item.data, '=');
		if (ptr == nullptr) continue;
		*ptr++ = '\0';
		if (strcmp(item.data, "topic_num") == 0) sscanf(ptr, "%d", &_topic_num);
		if (strcmp(item.data, "word_num") == 0) sscanf(ptr, "%d", &_word_num);
		if (strcmp(item.data, "alpha_m1") == 0) sscanf(ptr, "%lf", &_alpha_m1);
		if (strcmp(item.data, "beta_m1") == 0) sscanf(ptr, "%lf", &_beta_m1);
		if (strcmp(item.data, "beta_bg_m1") == 0) sscanf(ptr, "%lf", &_beta_bg_m1);
		if (strcmp(item.data, "gamma_m1") == 0) sscanf(ptr, "%lf", &_gamma_m1);
	}
}

void model::save_hyper_param(const char *path)
{
	FILE *fp = fopen(path, "w");
	fprintf(fp, "topic_num=%d\n", _topic_num);
	fprintf(fp, "word_num=%d\n", _word_num);
	fprintf(fp, "alpha_m1=%.20f\n", _alpha_m1);
	fprintf(fp, "beta_m1=%.20f\n", _beta_m1);
	fprintf(fp, "beta_bg_m1=%.20f\n", _beta_bg_m1);
	fprintf(fp, "gamma_m1=%.20f\n", _gamma_m1);
	fclose(fp);
}

void model::load_topic_param(const char *path)
{
	topic_param_file_reader reader(path, _word_num);
	_total_word_counts[0] = _total_word_counts[1] = 0;
	std::fill(_topic_all_word_counts, _topic_all_word_counts + _topic_num + 1, 0);
	for (int i = 0; i <= _topic_num; ++i)
	{
		file_item item = reader.get_item(false);
		assert((item.size != 0) && "Invalid topic parameter file");

		int *curr = _topic_word_counts[i];
		std::fill(curr, curr + _word_num, 0);
		utility::read_buffer buffer(item.data, item.size);
		buffer.read_sparse_array(curr, _word_num);
		long long sum = 0;
		for (int j = 0; j < _word_num; ++j) sum += curr[j];
		if (i < _topic_num)
		{
			_total_word_counts[1] += sum;
			_topic_all_word_counts[i] += sum;
		}
		else
		{
			_total_word_counts[0] += sum;
			_topic_all_word_counts[_topic_num] += sum;
		}
	}
}

void model::save_topic_param(const char *path)
{
	FILE *fp = fopen(path, "wb");
	utility::write_buffer buffer;
	for (int i = 0; i <= _topic_num; ++i)
	{
		buffer.clear();
		int *curr = _topic_word_counts[i];
		buffer.write_sparse_array(curr, _word_num, 0);
		utility::fwrite(buffer.buffer(), buffer.size(), fp);
		int nonzero_count = 0;
		long long sum = 0;
		for (int j = 0; j < _word_num; ++j) 
		{
			if (curr[j] > 0) ++nonzero_count;
			sum += curr[j];
		}
	}
	fclose(fp);
}

double model::topic_word_density()
{
	double sum = 0.0, num = 0.0;
	for (int i = 0; i < _word_num; ++i)
	{
		int topic_count = 0;
		long long word_count = 0;
		for (int j = 0; j < _topic_num; ++j)
		{
			if (_topic_word_counts[j][i] > 0)
			{
				++topic_count;
				word_count += _topic_word_counts[j][i];
			}
		}
		sum += (double)topic_count * word_count;
		num += word_count;
	}
	return sum / num;
}

double model::iterate(const char *tweet_path, size_t batch_size, const char *input_user_path, const char *input_tweet_path, const char *output_user_path, const char *output_tweet_path)
{
	tweet_file_reader tweet_reader(tweet_path, batch_size);
	user_param_file_reader user_param_reader(input_user_path, _topic_num);
	tweet_param_file_reader tweet_param_reader(input_tweet_path, batch_size);
	FILE *fp_user_param = fopen(output_user_path, "wb");
	FILE *fp_tweet_param = fopen(output_tweet_path, "wb");
	_user_indexes.clear();
	std::vector<char*> tweet_ptrs;
	std::vector<char*> tweet_param_ptrs;
	utility::write_buffer user_param_write_buffer;

	auto start_time = std::chrono::high_resolution_clock::now();
	long long process_word_count = 0, update_word_count = 0;
	while (true)
	{
		tweet_reader.trim();
		tweet_param_reader.trim();
		tweet_ptrs.clear();
		tweet_param_ptrs.clear();

		// read data batch
		while (true)
		{
			file_item tweet_item, tweet_param_item;
			if (tweet_ptrs.empty())
			{
				tweet_item = tweet_reader.get_item(false);
				tweet_param_item = tweet_param_reader.get_item(false);
				if (tweet_item.size == 0 || tweet_param_item.size == 0)
				{
					assert((tweet_item.size == tweet_param_item.size) && "Tweet data and param file endings not aligned");
					break;
				}
				tweet_ptrs.push_back(tweet_item.data);
				tweet_param_ptrs.push_back(tweet_param_item.data);
			}
			else
			{
				tweet_item = tweet_reader.get_item(true);
				if (tweet_item.size == 0) break;
				tweet_param_item = tweet_param_reader.get_item(true);
				if (tweet_param_item.size == 0)
				{
					tweet_reader.unget_item(tweet_item);
					break;
				}
			}
			tweet_ptrs.push_back(tweet_item.data + tweet_item.size);
			tweet_param_ptrs.push_back(tweet_param_item.data + tweet_param_item.size);

			int user;
			utility::get_varint(tweet_item.data, &user, tweet_item.size);
			if (_user_indexes.find(user) == _user_indexes.end()) // got new user in tweet data, read one more user parameter data
			{
				size_t user_index = _user_indexes.size();
				_user_indexes.insert(std::make_pair(user, (int)user_index));

				while (user_index >= _user_topic_counts.size())
				{
					_user_topic_counts.push_back(new int[_topic_num]);
					_user_all_topic_counts.push_back(0);
					_user_ids.push_back(-1);
				}

				file_item user_param_item = user_param_reader.get_item(false);
				assert((user_param_item.size != 0) && "User param file not aligned");

				utility::read_buffer user_param_buffer(user_param_item.data, user_param_item.size);
				int curr_user;
				user_param_buffer.read_varint(&curr_user);
				assert((curr_user == user) && "User param file not aligned");

				_user_ids[user_index] = user;

				int *topic_counts = _user_topic_counts[user_index];
				std::fill(topic_counts, topic_counts + _topic_num, 0);
				user_param_buffer.read_sparse_array(topic_counts, _topic_num);
				for (int i = 0; i < _topic_num; ++i)
				{
					if (topic_counts[i] < 0)
					{
						puts("oops");
					}
				}
				int all_topic_count = 0;
				for (int i = 0; i < _topic_num; ++i) all_topic_count += topic_counts[i];
				_user_all_topic_counts[user_index] = all_topic_count;
			}
		}

		if (tweet_ptrs.empty()) break;

		for (size_t i = 0; i < _thread_num; ++i)
		{
			size_t start = i * (tweet_ptrs.size() - 1) / _thread_num;
			size_t end = (i + 1) * (tweet_ptrs.size() - 1) / _thread_num;
			_tweet_read_buffers[i] = utility::read_buffer(tweet_ptrs[start], tweet_ptrs[end] - tweet_ptrs[start]);
			_tweet_param_read_buffers[i] = utility::read_buffer(tweet_param_ptrs[start], tweet_param_ptrs[end] - tweet_param_ptrs[start]);
			_tweet_param_write_buffers[i].clear();
		}

		// invoke worker threads for sampling
		parallel::_update();

		// update user, topic, and word counts
		for (size_t i = 0; i < _thread_num; ++i)
		{
			utility::read_buffer &tweet_buffer = _tweet_read_buffers[i];
			tweet_buffer.reset();
			utility::read_buffer &prev_tweet_param_buffer = _tweet_param_read_buffers[i];
			prev_tweet_param_buffer.reset();
			utility::read_buffer new_tweet_param_buffer(_tweet_param_write_buffers[i].buffer(), _tweet_param_write_buffers[i].size());
			utility::fwrite(_tweet_param_write_buffers[i].buffer(), _tweet_param_write_buffers[i].size(), fp_tweet_param);
			while (true)
			{
				int user;
				if (tweet_buffer.read_varint(&user) == 0) break;
				auto user_itor = _user_indexes.find(user);
				assert((user_itor != _user_indexes.end()) && "User not in buffer");

				int word_count;
				tweet_buffer.read_varint(&word_count);
				process_word_count += word_count;

				int user_index = user_itor->second;
				int prev_topic, prev_word_count;
				prev_tweet_param_buffer.read_varint(&prev_topic);
				prev_tweet_param_buffer.read_varint(&prev_word_count);
				assert((word_count == prev_word_count) && "Word counts not match in tweet data and previous param");

				int new_topic, new_word_count;
				new_tweet_param_buffer.read_varint(&new_topic);
				new_tweet_param_buffer.read_varint(&new_word_count);
				assert((word_count == new_word_count) && "Word counts not match in tweet data and new param");

				--_user_topic_counts[user_index][prev_topic];
				++_user_topic_counts[user_index][new_topic];

				for (int j = 0; j < word_count; j += 8)
				{
					char prev_tag, new_tag;
					prev_tweet_param_buffer.read(&prev_tag);
					new_tweet_param_buffer.read(&new_tag);
					for (int k = 0; k < 8 && j + k < word_count; ++k)
					{
						int word;
						tweet_buffer.read_varint(&word);
						if (prev_tag & (1 << k))
						{
							_dec_topic_word_count(prev_topic, word);
						}
						else
						{
							_dec_topic_word_count(_topic_num, word);
						}

						if (new_tag & (1 << k))
						{
							_inc_topic_word_count(new_topic, word);
						}
						else
						{
							_inc_topic_word_count(_topic_num, word);
						}

						if ((prev_tag & (1 << k)) != (new_tag & (1 << k)) || ((prev_tag & (1 << k)) && (new_tag & (1 << k)) && prev_topic != new_topic)) ++update_word_count;
					}
				}
			}
		}

		user_param_write_buffer.clear();
		size_t user_count = _user_indexes.size();
		for (size_t i = 0; i < user_count - 1; ++i)
		{
			user_param_write_buffer.write_varint(_user_ids[i]);
			user_param_write_buffer.write_sparse_array(_user_topic_counts[i], _topic_num);
			_user_indexes.erase(_user_ids[i]);
		}
		utility::fwrite(user_param_write_buffer.buffer(), user_param_write_buffer.size(), fp_user_param);
		if (user_count >= 1)
		{
			std::swap(_user_ids[0], _user_ids[user_count - 1]);
			std::swap(_user_topic_counts[0], _user_topic_counts[user_count - 1]);
			std::swap(_user_all_topic_counts[0], _user_all_topic_counts[user_count - 1]);
			_user_indexes[_user_ids[0]] = 0;
		}

		auto end_time = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
		printf("\r%.2f%% progress  %.4f update/word  %.2fk word/sec  %.1f sec  ", tweet_reader.position() * 100.0 / tweet_reader.size(), (double)update_word_count / process_word_count, (double)process_word_count / duration.count(), duration.count() * 0.001);
		fflush(stdout);
	}

	user_param_write_buffer.clear();
	for (size_t i = 0; i < _user_indexes.size(); ++i)
	{
		user_param_write_buffer.write_varint(_user_ids[i]);
		user_param_write_buffer.write_sparse_array(_user_topic_counts[i], _topic_num);
	}
	utility::fwrite(user_param_write_buffer.buffer(), user_param_write_buffer.size(), fp_user_param);

	fclose(fp_user_param);
	fclose(fp_tweet_param);
	
	printf("\n");
	fflush(stdout);

	return (double)update_word_count / process_word_count;
}

inline void model::_inc_topic_word_count(int topic, int word)
{
	++_topic_word_counts[topic][word];
	++_topic_all_word_counts[topic];
	if (topic == _topic_num)
	{
		++_total_word_counts[0];
	}
	else
	{
		++_total_word_counts[1];
	}
}

inline void model::_dec_topic_word_count(int topic, int word)
{
	--_topic_word_counts[topic][word];
	assert(_topic_word_counts[topic][word] >= 0);
	--_topic_all_word_counts[topic];
	assert(_topic_all_word_counts[topic] >= 0);
	if (topic == _topic_num) 
	{
		--_total_word_counts[0];
		assert(_total_word_counts[0] >= 0);
	}
	else
	{
		--_total_word_counts[1];
		assert(_total_word_counts[1] >= 0);
	}
}

static inline double pack_exp(double x, int x_exp)
{
	// assuming IEEE 754 format
	if (x_exp < -1022) return 0.0;
	if (x_exp > 1023) return std::numeric_limits<double>::infinity();
	unsigned int *e_ptr = (unsigned int*)&x + 1;
	*e_ptr = *e_ptr & 0x800fffff | ((x_exp + 1023) << 20);
	return x;
}

static inline void fix_exp(double &x, int &x_exp)
{
	// assuming IEEE 754 format
	unsigned int *e_ptr = (unsigned int*)&x + 1;
	int e = ((int)(*e_ptr >> 20) & 0x7ff) - 0x3ff;
	x_exp += e;
	*e_ptr = *e_ptr & 0x800fffff | 0x3ff00000;

	//while (x > prob_exp_base)
	//{
	//	x /= prob_exp_base;
	//	++x_exp;
	//}
	//while (x * prob_exp_base < 1.0)
	//{
	//	x *= prob_exp_base;
	//	--x_exp;
	//}
}

static inline void pow_fix_exp(double &x, int &x_exp, int n)
{
	double y = x;
	int y_exp = x_exp;
	x = 1.0;
	x_exp = 0;
	for (int i = 1; i <= n; i <<= 1)
	{
		if (i & n)
		{
			x *= y;
			x_exp += y_exp;
		}
		y *= y;
		y_exp += y_exp;
		fix_exp(y, y_exp);
	}
	fix_exp(x, x_exp);
}

void model::_sample(utility::read_buffer &tweet_read_buffer, utility::read_buffer &tweet_param_read_buffer, utility::write_buffer &tweet_param_write_buffer, std::default_random_engine &random_engine)
{
	std::uniform_real_distribution<double> uniform_distr(0.0, 1.0);

	std::vector<int> words, topic_words;
	std::vector<char> word_tags;

	double *topic_probs = new double[_topic_num];
	int *topic_prob_exps = new int[_topic_num];
	int *candidate_topics = new int[_topic_num];

	while (true)
	{
		int user;
		if (tweet_read_buffer.read_varint(&user) == 0) break;
		auto user_itor = _user_indexes.find(user);
		assert((user_itor != _user_indexes.end()) && "User not in buffer");

		int user_index = user_itor->second;

		int word_count;
		tweet_read_buffer.read_varint(&word_count);
		int prev_topic, param_word_count;
		tweet_param_read_buffer.read_varint(&prev_topic);
		tweet_param_read_buffer.read_varint(&param_word_count);
		assert((param_word_count == word_count) && "Tweet data and param are not aligned");

		word_tags.clear();
		topic_words.clear();
		words.clear();
		for (int i = 0; i < word_count; i += 8)
		{
			char tag;
			tweet_param_read_buffer.read(&tag);
			word_tags.push_back(tag);
			for (int j = 0; j < 8 && i + j < word_count; ++j)
			{
				int word;
				tweet_read_buffer.read_varint(&word);
				words.push_back(word);
				if (tag & (1 << j))
				{
					topic_words.push_back(word);
				}
			}
		}

		// sample user topic
		int max_prob_exp = std::numeric_limits<int>::min();

		candidate_topics[0] = prev_topic;
		for (int i = 0, j = 1; i < _topic_num; ++i)
		{
			if (i != prev_topic) candidate_topics[j++] = i;
		}

		for (int i = 0; i < _topic_num; ++i)
		{
			int topic = candidate_topics[i];
			double prob = (_user_topic_counts[user_index][topic] + _alpha_m1) / (_user_all_topic_counts[user_index] + _alpha_m1 * _topic_num); // theta(user, topic)
			int prob_exp = 0;
			for (size_t j = 0; j < topic_words.size(); ++j)
			{
				int word = topic_words[j];

				double phi = (_topic_word_counts[topic][word] + _beta_m1) / (_topic_all_word_counts[topic] + _beta_m1 * _word_num); // phi(topic, word)
				prob *= phi;

				if ((j & 15) == 15)
				{
					fix_exp(prob, prob_exp);
					if (prob_exp + 52 < max_prob_exp) break;
				}
			}
			
			fix_exp(prob, prob_exp);

			assert((prob > 0.0) && "Non-positive probability");

			topic_probs[topic] = prob;
			topic_prob_exps[topic] = prob_exp;
			if (max_prob_exp < prob_exp) max_prob_exp = prob_exp;
		}

		double topic_prob_sum = 0.0;
		for (int topic = 0; topic < _topic_num; ++topic)
		{
			topic_probs[topic] = pack_exp(topic_probs[topic], topic_prob_exps[topic] - max_prob_exp);
			topic_prob_sum += topic_probs[topic];
		}
		double topic_choice = uniform_distr(random_engine) * topic_prob_sum;
		topic_prob_sum = 0.0;
		int selected_topic = _topic_num - 1;
		for (int topic = 0; topic < _topic_num; ++topic)
		{
			topic_prob_sum += topic_probs[topic];
			if (topic_choice <= topic_prob_sum)
			{
				selected_topic = topic;
				break;
			}
		}

		tweet_param_write_buffer.write_varint(selected_topic);
		tweet_param_write_buffer.write_varint(word_count);

		// sample word whether in the selected topic or background topic
		for (int i = 0; i < word_count; i += 8)
		{
			char tag = 0;
			for (int j = 0; j < 8 && i + j < word_count; ++j)
			{
				int is_prev_selected = (word_tags[i / 8] & (1 << j)) ? 1 : 0;
				int is_prev_topic = (selected_topic == prev_topic) ? 1 : 0;
				int word = words[i + j];
				double pi0 = _total_word_counts[0] + _gamma_m1;
				double pi1 = _total_word_counts[1] + _gamma_m1;
				double phi0 = (_topic_word_counts[_topic_num][word] + _beta_bg_m1) / (_topic_all_word_counts[_topic_num] + _beta_bg_m1 * _word_num);
				double phi1 = (_topic_word_counts[selected_topic][word] + _beta_m1) / (_topic_all_word_counts[selected_topic] + _beta_m1 * _word_num);
				double prob0 = pi0 * phi0;
				double prob1 = pi1 * phi1;
				double word_choice = uniform_distr(random_engine) * (prob0 + prob1);
				if (word_choice > prob0)
				{
					tag |= 1 << j;
				}
			}
			tweet_param_write_buffer.write(tag);
		}
	}

	delete[] candidate_topics;
	delete[] topic_probs;
	delete[] topic_prob_exps;
}

void model::make_buffer(const char *input_path, const char *buffer_path, const char *user_path, const char *word_path, const char *tweet_id_path, const char *summary_path, const char *stopword_path, int min_user_freq, int min_word_freq)
{
	char default_user[] = "*";

	text_file_reader reader(input_path);

	printf("Building mappings...\n");

	std::unordered_map<char *, size_t, utility::string_hasher, utility::string_predicate> user_ids, word_ids;
	std::vector<char *> users, words;
	std::vector<int> user_counts, word_counts;
	while (true)
	{
		file_item item = reader.get_item(false);
		if (item.size == 0) break;

		char *ptr = strchr(item.data, '\t');
		char *user_str;
		if (ptr != nullptr)
		{
			*ptr++ = '\0';
			user_str = item.data;
		}
		else
		{
			ptr = item.data;
			user_str = default_user;
		}

		auto user_iterator = user_ids.find(user_str);
		if (user_iterator == user_ids.end())
		{
			char *user = utility::new_string(user_str);
			user_ids.insert(std::make_pair(user, users.size()));
			users.push_back(user);
			user_counts.push_back(1);
		}
		else
		{
			++user_counts[user_iterator->second];
		}

		while (*ptr)
		{
			char *p = strchr(ptr, ' ');
			if (p != nullptr) *p = '\0';
			auto word_iterator = word_ids.find(ptr);
			if (word_iterator == word_ids.end())
			{
				char *word = utility::new_string(ptr);
				word_ids.insert(std::make_pair(word, words.size()));
				words.push_back(word);
				word_counts.push_back(1);
			}
			else
			{
				++word_counts[word_iterator->second];
			}

			if (p == nullptr) break;
			ptr = p + 1;
		}
	}

	std::vector<int> user_indexes;
	for (size_t i = 0; i < users.size(); ++i) user_indexes.push_back((int)i);
	std::sort(user_indexes.begin(), user_indexes.end(), utility::index_comparer<std::vector<int>>(user_counts, true));
	user_ids.clear();
	FILE *fp_user = fopen(user_path, "w");
	for (size_t i = 0; i < user_indexes.size(); ++i)
	{
		size_t j = user_indexes[i];
		if (user_counts[j] < min_user_freq) break;
		user_ids.insert(std::make_pair(users[j], i));
		fprintf(fp_user, "%s\t%d\n", users[j], user_counts[j]);
	}
	fclose(fp_user);

	printf("%d users\n", (int)user_ids.size());

	if (stopword_path != nullptr)
	{
		text_file_reader stopword_reader(stopword_path);
		while (true)
		{
			file_item item = stopword_reader.get_item(false);
			if (item.size == 0) break;
			auto word_itor = word_ids.find(item.data);
			if (word_itor != word_ids.end()) word_ids.erase(word_itor);
		}
	}

	std::vector<int> word_indexes;
	for (auto word_itor : word_ids) word_indexes.push_back((int)word_itor.second);
	std::sort(word_indexes.begin(), word_indexes.end(), utility::index_comparer<std::vector<int>>(word_counts, true));
	word_ids.clear();
	FILE *fp_word = fopen(word_path, "w");
	for (size_t i = 0; i < word_indexes.size(); ++i)
	{
		size_t j = word_indexes[i];
		if (word_counts[j] < min_word_freq) break;
		word_ids.insert(std::make_pair(words[j], i));
		fprintf(fp_word, "%s\t%d\n", words[j], word_counts[j]);
	}
	fclose(fp_word);

	printf("%d words\n", (int)word_ids.size());
	printf("Building buffer...\n");

	FILE *fp_buffer = fopen(buffer_path, "wb");
	FILE *fp_tweet_id = fopen(tweet_id_path, "wb");

	reader.reset();

	long long valid_tweet_count = 0, total_tweet_count = 0;
	std::vector<int> word_buffer;
	utility::write_buffer tweet_buffer, tweet_id_buffer;
	while (true)
	{
		file_item item = reader.get_item(false);
		if (item.size == 0) break;
		++total_tweet_count;

		char *ptr = strchr(item.data, '\t');
		char *user_str;
		if (ptr != nullptr)
		{
			*ptr++ = '\0';
			user_str = item.data;
		}
		else
		{
			ptr = item.data;
			user_str = default_user;
		}

		auto user_iterator = user_ids.find(user_str);
		if (user_iterator == user_ids.end()) continue;
		int user_id = (int)user_iterator->second;

		word_buffer.clear();
		while (*ptr)
		{
			char *p = strchr(ptr, ' ');
			if (p != nullptr) *p = '\0';
			auto word_iterator = word_ids.find(ptr);
			if (word_iterator != word_ids.end())
			{
				int word = (int)word_iterator->second;
				word_buffer.push_back(word);
			}
			if (p == nullptr) break;
			ptr = p + 1;
		}

		if (word_buffer.empty()) continue;
		int word_count = (int)word_buffer.size();

		tweet_buffer.clear();
		tweet_buffer.write_varint(user_id);
		tweet_buffer.write_varint(word_count);
		for (int i = 0; i < word_count; ++i)
		{
			tweet_buffer.write_varint(word_buffer[i]);
		}

		utility::fwrite(tweet_buffer.buffer(), tweet_buffer.size(), fp_buffer);
		
		tweet_id_buffer.clear();
		tweet_id_buffer.write_varint(total_tweet_count - 1);
		utility::fwrite(tweet_id_buffer.buffer(), tweet_id_buffer.size(), fp_tweet_id);

		++valid_tweet_count;
	}
	fclose(fp_buffer);
	fclose(fp_tweet_id);

	for (size_t i = 0; i < users.size(); ++i) delete[] users[i];
	for (size_t i = 0; i < words.size(); ++i) delete[] words[i];

	printf("%lld / %lld tweets\n", valid_tweet_count, total_tweet_count);

	FILE *fp_summary = fopen(summary_path, "w");
	fprintf(fp_summary, "word_num=%d\n", (int)word_ids.size());
	fprintf(fp_summary, "user_num=%d\n", (int)user_ids.size());
	fprintf(fp_summary, "valid_tweet_num=%lld\n", valid_tweet_count);
	fprintf(fp_summary, "total_tweet_num=%lld\n", total_tweet_count);
	fclose(fp_summary);
}

void model::save_user_topic_distribution(const char *user_param_path, const char *output_path)
{
	user_param_file_reader reader(user_param_path, _topic_num);
	FILE *fp = fopen(output_path, "wb");
	int *buffer = new int[_topic_num * 2 + 2];
	int *topic_counts = new int[_topic_num];
	while (true)
	{
		file_item item = reader.get_item(false);
		if (item.size == 0) break;
		utility::read_buffer user_buffer(item.data, item.size);
		int user;
		user_buffer.read(&user);
		std::fill(topic_counts, topic_counts + _topic_num, 0);
		user_buffer.read_sparse_array(topic_counts, _topic_num);

		int topic_count = 0;
		for (int i = 0; i < _topic_num; ++i)
		{
			if (topic_counts[i] == 0) continue;
			buffer[topic_count * 2 + 2] = topic_count;
			buffer[topic_count * 2 + 3] = topic_counts[i];
			++topic_count;
		}
		buffer[0] = user;
		buffer[1] = topic_count;

		utility::fwrite(buffer, topic_count * 2 + 2, fp);
	}
	delete[] buffer;
	delete[] topic_counts;
	fclose(fp);
}

void model::save_topic_word_distribution(const char *output_path)
{
	FILE *fp = fopen(output_path, "wb");
	int *buffer = new int[_word_num * 2 + 1];
	for (int i = 0; i <= _topic_num; ++i)
	{
		int word_count = 0;
		for (int j = 0; j < _word_num; ++j)
		{
			if (_topic_word_counts[i][j] == 0) continue;
			buffer[word_count * 2 + 1] = j;
			buffer[word_count * 2 + 2] = _topic_word_counts[i][j];
			++word_count;
		}
		buffer[0] = word_count;
		utility::fwrite(buffer, word_count * 2 + 1, fp);
	}
	delete[] buffer;
	fclose(fp);
}

void model::save_user_topic_distribution_text(const char *user_param_path, const char *user_path, const char *output_path)
{
	std::vector<char*> users;
	text_file_reader user_reader(user_path);
	while (true)
	{
		file_item item = user_reader.get_item(false);
		if (item.size == 0) break;
		char *ptr = strchr(item.data, '\t');
		if (ptr != nullptr) *ptr = '\0';
		users.push_back(utility::new_string(item.data));
	}

	int *topics = new int[_topic_num];
	for (int i = 0; i < _topic_num; ++i) topics[i] = i;
	int *topic_counts = new int[_topic_num];
	std::fill(topic_counts, topic_counts + _topic_num, 0);
	FILE *fp = fopen(output_path, "w");
	user_param_file_reader user_param_reader(user_param_path, _topic_num);
	while (true)
	{
		file_item item = user_param_reader.get_item(false);
		if (item.size == 0) break;
		
		utility::read_buffer buffer(item.data, item.size);
		int user;
		buffer.read_varint(&user);
		buffer.read_sparse_array(topic_counts, _topic_num);
		std::sort(topics, topics + _topic_num, utility::index_comparer<int*>(topic_counts, true));
		fprintf(fp, "%s", users[user]);
		for (int i = 0; i < _topic_num; ++i)
		{
			int topic = topics[i];
			if (topic_counts[topic] == 0) break;
			fprintf(fp, "\t%d %d", topic, topic_counts[topic]);
			topic_counts[topic] = 0;
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
	delete[] topic_counts;
	delete[] topics;
	for (size_t i = 0; i < users.size(); ++i) delete[] users[i];
}

void model::save_topic_word_distribution_text(const char *word_path, const char *output_path)
{
	std::vector<char*> words;
	text_file_reader reader(word_path);
	while (true)
	{
		file_item item = reader.get_item(false);
		if (item.size == 0) break;
		char *ptr = strchr(item.data, '\t');
		if (ptr != nullptr) *ptr = '\0';
		words.push_back(utility::new_string(item.data));
	}
	if (words.size() != (size_t)_word_num)
	{
		printf("Invalid word file\n");
		return;
	}

	int *indexes = new int[_word_num];
	for (int i = 0; i < _word_num; ++i) indexes[i] = i;
	FILE *fp = fopen(output_path, "w");
	for (int i = 0; i <= _topic_num; ++i)
	{
		std::sort(indexes, indexes + _word_num, utility::index_comparer<int*>(_topic_word_counts[i], true));
		fprintf(fp, "%d", i);
		for (int j = 0; j < _word_num; ++j)
		{
			int k = indexes[j];
			int count = _topic_word_counts[i][k];
			if (count == 0) break;
			fprintf(fp, "\t%s %d", words[k], count);
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
	delete[] indexes;
	for (size_t i = 0; i < words.size(); ++i) delete[] words[i];
}

void model::save_tweet_topic_text(const char *tweet_param_path, const char *tweet_path, const char *tweet_id_path, const char *output_path)
{
	tweet_param_file_reader tweet_param_reader(tweet_param_path);
	text_file_reader tweet_reader(tweet_path);
	tweet_id_file_reader tweet_id_reader(tweet_id_path);
	long long tweet_count = 0;
	FILE *fp = fopen(output_path, "w");
	while (true)
	{
		file_item tweet_param_item = tweet_param_reader.get_item(false);
		file_item tweet_id_item = tweet_id_reader.get_item(false);
		if (tweet_param_item.size == 0 || tweet_id_item.size == 0)
		{
			if (tweet_param_item.size != tweet_id_item.size)
			{
				printf("Tweet file and parameter file not match\n");
			}
			break;
		}
		utility::read_buffer tweet_param_buffer(tweet_param_item.data, tweet_param_item.size);
		int topic;
		tweet_param_buffer.read_varint(&topic);
		utility::read_buffer tweet_id_buffer(tweet_id_item.data, tweet_id_item.size);
		long long tweet_id;
		tweet_id_buffer.read_varint(&tweet_id);

		while (tweet_count <= tweet_id)
		{
			file_item tweet_item = tweet_reader.get_item(false);
			if (tweet_item.size == 0)
			{
				printf("Unexpected end in tweet file\n");
				break;
			}
			++tweet_count;

			fprintf(fp, "%d\t%s\n", (tweet_count == tweet_id + 1) ? topic : -1, tweet_item.data);
		}

		if (tweet_count != tweet_id + 1)
		{
			printf("Tweet file and id file not match\n");
			break;
		}
	}
	fclose(fp);
}

int model::infer(std::vector<int> &words, infer_mode mode, double *probs)
{
	int selected_topic = -1;
	if (mode == infer_mode::score)
	{
		double max_score = 0.0;
		for (int topic = 0; topic < _topic_num; ++topic)
		{
			double score = 0.0;
			int *word_counts = _topic_word_counts[topic];
			for (size_t i = 0; i < words.size(); ++i)
			{
				score += word_counts[words[i]];
			}
			score = (score + _beta_m1 * words.size()) / (_topic_all_word_counts[topic] + _beta_m1 * _word_num);
			if (max_score < score)
			{
				max_score = score;
				selected_topic = topic;
			}
			if (probs != nullptr) probs[topic] = score;
		}
		return selected_topic;
	}
	else if (mode == infer_mode::probability)
	{
		double max_prob = 0.0;
		int max_prob_exp = std::numeric_limits<int>::min();
		for (int topic = 0; topic < _topic_num; ++topic)
		{
			double prob = 1.0;
			int prob_exp = 0;
			int *word_counts = _topic_word_counts[topic];
			double sum = _topic_all_word_counts[topic] + _beta_m1 * _word_num;
			for (size_t i = 0; i < words.size(); ++i)
			{
				prob *= (word_counts[words[i]] + _beta_m1) / sum;
				if ((i & 15) == 15)
				{
					fix_exp(prob, prob_exp);
					if (prob_exp + 52 < max_prob_exp) break;
				}
			}
			fix_exp(prob, prob_exp);

			if (max_prob_exp < prob_exp || (max_prob_exp == prob_exp && max_prob < prob))
			{
				max_prob = prob;
				max_prob_exp = prob_exp;
				selected_topic = topic;
			}
		}

		if (probs != nullptr)
		{
			for (int topic = 0; topic < _topic_num; ++topic)
			{
				double prob = 1.0;
				int prob_exp = 0;
				int *word_counts = _topic_word_counts[topic];
				double sum = _topic_all_word_counts[topic] + _beta_m1 * _word_num;
				for (size_t i = 0; i < words.size(); ++i)
				{
					prob *= (word_counts[words[i]] + _beta_m1) / sum;
					if ((i & 15) == 15)
					{
						fix_exp(prob, prob_exp);
						if (prob_exp + 52 < max_prob_exp) break;
					}
				}
				fix_exp(prob, prob_exp);
				probs[topic] = pack_exp(prob, prob_exp - max_prob_exp);
			}
		}
	}
	
	if (probs != nullptr)
	{
		double sum = 0.0;
		for (int i = 0; i < _topic_num; ++i) sum += probs[i];
		if (sum != 0.0)
		{
			for (int i = 0; i < _topic_num; ++i) probs[i] /= sum;
		}
	}

	return selected_topic;
}