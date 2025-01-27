import argparse
import os
import sys
import multiprocessing
import math 

import csv
import matplotlib as mpl
# mpl.use('Agg')
mpl.rcParams['grid.linestyle'] = ":"
mpl.rcParams.update({'font.size': 20})
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
import numpy as np
import statistics as st
from dataset_info import mk, dsinfo, data_options



parser = argparse.ArgumentParser()
parser.add_argument("-type", help="Specify which type of experiment: visited, topk, ratio")
parser.add_argument("-step", help="Specify which step to measure at")
parser.add_argument("-dataset", help="dataset")
parser.add_argument("-g","--graphs_only", help="graphs only",action="store_true")
parser.add_argument("-p","--paper_ver", help="paper_version",action="store_true")
parser.add_argument("-f", "--filter", help="filter out candidates who have already found a match", action="store_true")
parser.add_argument("-graph_name", help="graphs name")

args = parser.parse_args()
print("Dataset: " + args.dataset)
dataset = args.dataset

if args.type == "visited":
  executable = "../rangeSearch/vamanaRange/range_visited"
elif args.type == "top1":
  executable = "../rangeSearch/vamanaRange/range_top1"
elif args.type == "top10":
  executable = "../rangeSearch/vamanaRange/range_top10"
elif args.type == "ratio":
  executable = "../rangeSearch/vamanaRange/range_ratio"


already_ran = set()

def runstring(op, outfile):
    if op in already_ran:
        return
    already_ran.add(op)
    os.system("echo \"" + op + "\"")
    os.system("echo \"" + op + "\" >> " + outfile)
    x = os.system(op + " >> " + outfile)
    if (x) :
        if (os.WEXITSTATUS(x) == 0) : raise NameError("  aborted: " + op)
        os.system("echo Failed")
    
def runtest(dataset_name, outfile) :
    ds = data_options[dataset_name]
    op = executable
    op += " -base_path " + ds["base"] 
    op += " -gt_path " + ds["gt"] 
    op += " -query_path " + ds["query"] 
    op += " -data_type " + ds["data_type"] 
    op += " -dist_func " + ds["dist_fn"] 
    op += " -r " + str(ds["radius"] )
    op += " -early_stopping_radius " + str(ds["esr"] )
    op += " -alpha " + str(ds["alpha"] )
    op += " -R " + str(64)
    op += " -L " + str(128)
    runstring(op, outfile)

def string_to_list(s):
  s = s.strip().strip('[').strip(']').split(',')
  return [ss.strip() for ss in s]



if not args.graphs_only:
    directory = "dist_histograms/" + args.type + "/"
    os.makedirs(directory, exist_ok=True)
    outFile = directory + "/" + str(dataset) + ".txt"
    # clear output file
    os.system("echo \"\" > " + outFile)
    runtest(dataset, outFile)


'''
Results file finds lists of distances for the specified step for each of the three groups
'''
def readResultsFile(result_filename, step, filter):
    file = open(result_filename, 'r')
    lists = []
    if not filter:
      zeros_line = "Zeros, Step " + step + ":"
      onetwos_line = "OneTwos, Step " + step + ":"
      threeplus_line = "Threeplus, Step " + step + ":"
    else:
      zeros_line = "Zeros, Step " + step + ", Filtered:"
      onetwos_line = "OneTwos, Step " + step + ", Filtered:"
      threeplus_line = "Threeplus, Step " + step + ", Filtered:"

    for line in file.readlines():
      if line.find(zeros_line) != -1:
          zeros = line.split(': ')[1]
          zeros = eval(zeros)
          lists.append(zeros)
      if line.find(onetwos_line) != -1:
          onetwos = line.split(': ')[1]
          onetwos = eval(onetwos)
          lists.append(onetwos)
      if line.find(threeplus_line) != -1:
          threeplus = line.split(': ')[1]
          threeplus = eval(threeplus)
          lists.append(threeplus)
    return lists

def export_legend(legend, filename="legend.pdf"):
    fig  = legend.figure
    fig.canvas.draw()
    bbox  = legend.get_window_extent().transformed(fig.dpi_scale_trans.inverted())
    fig.savefig(filename, dpi="figure", bbox_inches=bbox)

def normalize_ratio_arr(ratio_arr):
  ratio_arr_normalized = np.zeros(len(ratio_arr))
  for i in range(len(ratio_arr)):
    ratio_arr_normalized[i] = 2-ratio_arr[i]
  return ratio_arr_normalized


def plot_dist_histogram(dataset, graph_name, paper_ver, filter):
  if not filter:
    dir_name = "graphs/dist_histograms/" + args.type + "/" + "Step" + args.step
  else:
    dir_name = "graphs/dist_histograms/" + args.type + "_Filtered" + "/" + "Step" + args.step
  os.makedirs(dir_name, exist_ok=True)

  outputFile = dir_name + "/" + graph_name.replace('.', '') + '.pdf'
  mpl.rcParams.update({'font.size': 25})

  resultsFile = "dist_histograms/" + args.type  + "/" + dataset + ".txt"
  lists = readResultsFile(resultsFile, args.step, filter)

  zeros = np.array(lists[0])
  onetwos = np.array(lists[1])
  threeplus = np.array(lists[2])

  if (dataset == "msmarco-1M") and (args.type == "ratio") :
    zeros = normalize_ratio_arr(zeros)
    onetwos = normalize_ratio_arr(onetwos)
    threeplus = normalize_ratio_arr(threeplus)

  print("Num zero results:", len(zeros))
  print("Num onetwos", len(onetwos))
  print("Num threeplus:", len(threeplus))

  if args.type == "ratio":
    plt.xlabel("d_top10 / d_start")
  elif args.type == "visited":
    plt.xlabel("d_visited")
  elif args.type == "top10":
    plt.xlabel("d_top10")
  elif args.type == "top1":
    plt.xlabel("d_top1")
  
  plt.ylabel("Frequency")
  if len(zeros) != 0:
    plt.hist(zeros, bins=math.ceil(len(zeros)/5), alpha=.5, label = 'Zero Results')
  if len(onetwos) != 0:
    plt.hist(onetwos, bins=math.ceil(len(onetwos)/5), alpha=.5, label = '1-2 Results')
  if len(threeplus) != 0:
    plt.hist(threeplus, bins=math.ceil(len(threeplus)/5), alpha=.5, label = '3+ Results')

  if filter:
    esr = 0
    ds = data_options[dataset]
    if args.type == "ratio":
      esr = ds["esr_ratio"]
    elif args.type == "visited":
      esr = ds["esr"]
    elif args.type == "top1":
      esr = ds["esr_top1"]
    elif args.type == "top10":
      esr = ds["esr_top10"]
    if esr != 0:
      plt.axvline(x=esr, color='black', linestyle='--')


  legend_x = 1
  legend_y = 0.5 
  plt.grid()
  if not paper_ver:
    plt.title(graph_name)
    plt.legend(loc='center left', bbox_to_anchor=(legend_x, legend_y))
  plt.savefig(outputFile, bbox_inches='tight')

  if paper_ver:
    nc = 8
    legend = plt.legend(loc='center left', bbox_to_anchor=(legend_x, legend_y), ncol=nc, framealpha=0.0)
    export_legend(legend, dir_name + "/" + graph_name + '_legend.pdf')
  plt.close('all')


plot_dist_histogram(dataset, args.graph_name, args.paper_ver, args.filter)





