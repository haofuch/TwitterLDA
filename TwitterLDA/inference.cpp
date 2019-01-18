#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cstring>
#include <chrono>
#include "inference.h"
#include "model.h"
#include "parallel.h"
#include "utility.h"
#include "file_reader.h"

inference::inference(model &m, model::infer_mode mode, const char *word_path, size_t thread_num) : parallel(thread_num), _m(m)
{
	_mode = mode;
	text_file_reader reader(word_path);
	while (true)
	{
		file_item item = reader.get_item(false);
		if (item.size == 0) break;
		char *ptr = strchr(item.data, '\t');
		if (ptr != nullptr) *ptr = '\0';
		char *word = utility::new_string(item.data);
		_words.push_back(word);
		_word_ids.insert(std::make_pair(word, (int)_word_ids.size()));
	}
}

inference::~inference()
{
	for (size_t i = 0; i < _words.size(); ++i) delete[] _words[i];
}

void inference::infer(const char *input_path, size_t batch_size, const char *output_path)
{
	text_file_reader reader(input_path, batch_size);
	FILE *fp = fopen(output_path, "w");

	auto start_time = std::chrono::high_resolution_clock::now();
	long long process_tweet_count = 0;
	while (true)
	{
		_input_ptrs.clear();
		reader.trim();
		while (true)
		{
			file_item item = reader.get_item(!_input_ptrs.empty());
			if (item.size == 0) break;
			_input_ptrs.push_back(item.data);
		}

		if (_input_ptrs.empty()) break;

		_output_topics.resize(_input_ptrs.size());
		_output_probs.resize(_input_ptrs.size());

		parallel::_update();

		for (size_t i = 0; i < _input_ptrs.size(); ++i)
		{
			fprintf(fp, "%d\t%f\t%s\n", _output_topics[i], _output_probs[i], _input_ptrs[i]);
		}

		process_tweet_count += _input_ptrs.size();
		auto end_time = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
		printf("\r%.2f%% progress  %.2fk tweet/sec  %.1f sec  ", reader.position() * 100.0 / reader.size(), (double)process_tweet_count / duration.count(), duration.count() * 0.001);
		fflush(stdout);
	}
	fclose(fp);

	printf("\n");
	fflush(stdout);
}

void inference::_update(size_t id)
{
	size_t start = id * _input_ptrs.size() / _thread_num;
	size_t end = (id + 1) * _input_ptrs.size() / _thread_num;
	_infer(start, end);
}

void inference::_infer(size_t start, size_t end)
{
	std::vector<int> words;
	double *topic_probs = new double[_m.topic_num()];
	for (size_t i = start; i < end; ++i)
	{
		words.clear();
		char *ptr = _input_ptrs[i];
		char *p = strchr(ptr, '\t');
		if (p != nullptr) ptr = p + 1;
		while (true)
		{
			p = strchr(ptr, ' ');
			if (p != nullptr) *p = '\0';
			auto iter = _word_ids.find(ptr);
			if (iter != _word_ids.end()) words.push_back(iter->second);
			if (p == nullptr)
			{
				ptr += strlen(ptr) + 1;
				break;
			}
			*p = ' ';
			ptr = p + 1;
		}
		int topic = _m.infer(words, _mode, topic_probs);
		_output_topics[i] = topic;
		_output_probs[i] = (topic < 0) ? 0.0 : topic_probs[topic];
	}
	delete[] topic_probs;
}
