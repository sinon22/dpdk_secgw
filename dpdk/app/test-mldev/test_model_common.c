/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <errno.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_mldev.h>

#include "ml_common.h"
#include "test_model_common.h"

int
ml_model_load(struct ml_test *test, struct ml_options *opt, struct ml_model *model, uint16_t fid)
{
	struct test_common *t = ml_test_priv(test);
	struct rte_ml_model_params model_params;
	FILE *fp;
	int ret;

	if (model->state == MODEL_LOADED)
		return 0;

	if (model->state != MODEL_INITIAL)
		return -EINVAL;

	/* read model binary */
	fp = fopen(opt->filelist[fid].model, "r");
	if (fp == NULL) {
		ml_err("Failed to open model file : %s\n", opt->filelist[fid].model);
		return -1;
	}

	fseek(fp, 0, SEEK_END);
	model_params.size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	model_params.addr = rte_malloc_socket("ml_model", model_params.size,
					      t->dev_info.min_align_size, opt->socket_id);
	if (model_params.addr == NULL) {
		ml_err("Failed to allocate memory for model: %s\n", opt->filelist[fid].model);
		fclose(fp);
		return -ENOMEM;
	}

	if (fread(model_params.addr, 1, model_params.size, fp) != model_params.size) {
		ml_err("Failed to read model file : %s\n", opt->filelist[fid].model);
		rte_free(model_params.addr);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	/* load model to device */
	ret = rte_ml_model_load(opt->dev_id, &model_params, &model->id);
	if (ret != 0) {
		ml_err("Failed to load model : %s\n", opt->filelist[fid].model);
		model->state = MODEL_ERROR;
		rte_free(model_params.addr);
		return ret;
	}

	/* release mz */
	rte_free(model_params.addr);

	/* get model info */
	ret = rte_ml_model_info_get(opt->dev_id, model->id, &model->info);
	if (ret != 0) {
		ml_err("Failed to get model info : %s\n", opt->filelist[fid].model);
		return ret;
	}

	/* Update number of batches */
	if (opt->batches == 0)
		model->nb_batches = model->info.batch_size;
	else
		model->nb_batches = opt->batches;

	model->state = MODEL_LOADED;

	return 0;
}

int
ml_model_unload(struct ml_test *test, struct ml_options *opt, struct ml_model *model, uint16_t fid)
{
	struct test_common *t = ml_test_priv(test);
	int ret;

	RTE_SET_USED(t);

	if (model->state == MODEL_INITIAL)
		return 0;

	if (model->state != MODEL_LOADED)
		return -EINVAL;

	/* unload model */
	ret = rte_ml_model_unload(opt->dev_id, model->id);
	if (ret != 0) {
		ml_err("Failed to unload model: %s\n", opt->filelist[fid].model);
		model->state = MODEL_ERROR;
		return ret;
	}

	model->state = MODEL_INITIAL;

	return 0;
}

int
ml_model_start(struct ml_test *test, struct ml_options *opt, struct ml_model *model, uint16_t fid)
{
	struct test_common *t = ml_test_priv(test);
	int ret;

	RTE_SET_USED(t);

	if (model->state == MODEL_STARTED)
		return 0;

	if (model->state != MODEL_LOADED)
		return -EINVAL;

	/* start model */
	ret = rte_ml_model_start(opt->dev_id, model->id);
	if (ret != 0) {
		ml_err("Failed to start model : %s\n", opt->filelist[fid].model);
		model->state = MODEL_ERROR;
		return ret;
	}

	model->state = MODEL_STARTED;

	return 0;
}

int
ml_model_stop(struct ml_test *test, struct ml_options *opt, struct ml_model *model, uint16_t fid)
{
	struct test_common *t = ml_test_priv(test);
	int ret;

	RTE_SET_USED(t);

	if (model->state == MODEL_LOADED)
		return 0;

	if (model->state != MODEL_STARTED)
		return -EINVAL;

	/* stop model */
	ret = rte_ml_model_stop(opt->dev_id, model->id);
	if (ret != 0) {
		ml_err("Failed to stop model: %s\n", opt->filelist[fid].model);
		model->state = MODEL_ERROR;
		return ret;
	}

	model->state = MODEL_LOADED;

	return 0;
}
