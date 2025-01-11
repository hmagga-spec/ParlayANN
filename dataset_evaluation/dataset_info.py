import argparse
import os
import sys
import multiprocessing
import math 



bigann = "/ssd1/data/bigann"
msturing= "/ssd1/data/MSTuringANNS"
deep = "/ssd1/data/deep1b"
gist="/ssd1/data/gist"
ssnpp="/ssd1/data/FB_ssnpp"
wikipedia="/ssd1/data/wikipedia_cohere"
msmarco="/ssd1/data/msmarco_websearch"
text2image="/ssd1/data/text2image1B"
openai="/ssd1/data/OpenAIArXiv"
data_options = {
  "bigann-1M" : {"base": bigann+"/base.1B.u8bin.crop_nb_1000000", 
                "query": bigann+"/query.public.10K.u8bin", 
                "data_type" : "uint8", 
                "dist_fn": "Euclidian",
                "radius": 10000,
                "esr": 11000,
                "alpha": 1.15,
                "gt": bigann+"/range_gt_1M_10000"},
  "bigann-10M" : {"base": bigann+"/base.1B.u8bin.crop_nb_10000000", 
                "query": bigann+"/query.public.10K.u8bin", 
                "data_type" : "uint8", 
                "dist_fn": "Euclidian",
                "radius": 10000,
                "esr": 11000,
                "alpha": 1.15,
                "gt": bigann+"/range_gt_10M_10000"},
  "bigann-100M" : {"base": bigann+"/base.1B.u8bin.crop_nb_100000000", 
                "query": bigann+"/query.public.10K.u8bin", 
                "data_type" : "uint8", 
                "dist_fn": "Euclidian",
                "radius": 10000,
                "esr": 11000,
                "alpha": 1.15,
                "gt": bigann+"/range_gt_100M_10000"},
  "msturing-1M" : {"base": msturing+"/base1b.fbin.crop_nb_1000000", 
                "query": msturing+"/query10K.fbin", 
                "data_type" : "float", 
                "dist_fn": "Euclidian",
                "radius": .3,
                "esr": .4,
                "alpha": 1.15,
                "gt": msturing+"/range_gt_1M_100K_.3"},
  "deep-1M" : {"base": deep+"/base.1B.fbin.crop_nb_1000000", 
                "query": deep+"/query.public.10K.fbin", 
                "data_type" : "float",
                "dist_fn": "Euclidian",
                "radius": .02,
                "esr": .03,
                "alpha": 1.15,
                "gt": deep+"/range_gt_1M_.02"},
  "gist-1M" : {"base": gist+"/gist_base.fbin", 
                "query": gist+"/gist_query.fbin", 
                "data_type" : "float", 
                "dist_fn": "Euclidian",
                "radius": .5,
                "esr": .6,
                "alpha": 1.15,
                "gt": gist+"/range_gt_1M_.5"},
  "ssnpp-1M" : {"base": ssnpp+"/FB_ssnpp_database.u8bin.crop_nb_1000000", 
                "query": ssnpp+"/FB_ssnpp_public_queries.u8bin", 
                "data_type" : "uint8", 
                "dist_fn": "Euclidian",
                "radius": 96237,
                "esr": 200000,
                "alpha": 1.10,
                "gt": ssnpp+"/ssnpp-1M"},
  "ssnpp-10M" : {"base": ssnpp+"/FB_ssnpp_database.u8bin.crop_nb_10000000", 
                "query": ssnpp+"/FB_ssnpp_public_queries.u8bin", 
                "data_type" : "uint8", 
                "dist_fn": "Euclidian",
                "radius": 96237,
                "esr": 200000,
                "alpha": 1.10,
                "gt": ssnpp+"/ssnpp-10M"},
  "ssnpp-100M" : {"base": ssnpp+"/FB_ssnpp_database.u8bin.crop_nb_100000000", 
                "query": ssnpp+"/FB_ssnpp_public_queries.u8bin", 
                "data_type" : "uint8", 
                "dist_fn": "Euclidian",
                "radius": 96237,
                "esr": 200000,
                "alpha": 1.10,
                "gt": ssnpp+"/ssnpp-100M"},
  "wikipedia-1M" : {"base":wikipedia+"/wikipedia_base.bin.crop_nb_1000000", 
                "query": wikipedia+"/wikipedia_query.bin", 
                "data_type" : "float", 
                "dist_fn": "mips",
                "radius": -10.5,
                "esr": -10.4, #final choice 
                "alpha": 1.0, 
                "gt": wikipedia+"/range_gt_1M_-10.5"},
  "msmarco-1M" : {"base": msmarco+"/vectors.bin.crop_nb_1000000", 
                "query": msmarco+"/query.bin", 
                "data_type" : "float", 
                "dist_fn": "mips",
                "radius": -62,
                "esr": -60, #final choice
                "alpha": 1.0,
                "gt": msmarco+"/range_gt_1M_-62"},
  "text2image-1M" : {"base": text2image+"/base.1B.fbin.crop_nb_1000000", 
                "query": text2image+"/query.public.100K.fbin", 
                "data_type" : "float", 
                "dist_fn": "mips",
                "radius": -.60,
                "esr": -.58, 
                "alpha": 1.0,
                "gt": text2image+"/range_gt_1M_-.6"},
  "openai-1M" : {"base": openai+"/openai_base_1M.bin", 
                "query": openai+"/openai_query_10K.bin", 
                "data_type" : "float", 
                "dist_fn": "Euclidian",
                "radius": .2,
                "esr": .25, #final choice 
                "alpha": 1.15,
                "gt": openai+"/openai_gt_1M_.2"},
}

mk = ['o', 'v', '^', '1', 's', '+', 'x', 'D', '|', '>', '<',]

dsinfo = {
  "bigann-1M" : {"marker": mk[0], 
                "color": "C0"},
  "bigann-10M" : {"marker": mk[0], 
                "color": "C0",
                "alpha": .7},
  "bigann-100M" : {"marker": mk[0], 
                "color": "C0",
                "alpha": .4},
  "msturing-1M" : {"marker": mk[1], 
                "color": "C1"},
  "deep-1M" : {"marker": mk[2], 
                "color": "C2"},
  "gist-1M" : {"marker": mk[3], 
                "color": "C3"},
  "ssnpp-1M" : {"marker": mk[4], 
                "color": "C4"},
  "ssnpp-10M" : {"marker": mk[4], 
                "color": "C4",
                "alpha": .7},
  "ssnpp-100M" : {"marker": mk[4], 
                "color": "C4",
                "alpha": .4},
  "wikipedia-1M" : {"marker": mk[5], 
                "color": "C5"},
  "wikipedia-10M" : {"marker": mk[5], 
                "color": "C5",
                "alpha": .7},
  "msmarco-1M" : {"marker": mk[6], 
                "color": "C6"},
  "text2image-1M" : {"marker": mk[7], 
                "color": "C7"},
  "openai-1M" : {"marker": mk[8], 
                "color": "C8"},
}


