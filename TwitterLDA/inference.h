#pragma once
#include <unordered_map>
#include <vector>
#include "model.h"
#include "parallel.h"
#include "utility.h"

class inference : public parallel
{
public:
	inference(model &m, model::infer_mode mode, const char *word_path, size_t thread_num);
	~inference();

	void infer(const char *input_path, size_t batch_size, const char *output_path);

private:
	model &_m;
	std::unordered_map<char*, int, utility::string_hasher, utility::string_predicate> _word_ids;
	std::vector<char*> _words;
	std::vector<char*> _input_ptrs;
	std::vector<int> _output_topics;
	std::vector<double> _output_probs;
	model::infer_mode _mode;

	void _update(size_t id);
	void _infer(size_t start, size_t end);
};

