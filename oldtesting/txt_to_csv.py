#!/usr/bin/python

import csv
import sys

outCsv=csv.writer(sys.stdout)

found = False;
while not found :
  line = sys.stdin.readline().strip()
  if line.startswith('template-storage'):
    found = True

outCsv.writerow( ('#seq', ' freq_ratio', ' harmonic', ' amp_ratio', ' tracknum', ' f', ' amp', ' newflag', ' env', ' pitch', 'note' ) )
row = []
for line in sys.stdin.readlines():
  line=line.strip()
  # remove trailing ;
  if line.endswith(';'):
    line=line[0:-1]
    isEndOfRecord = True
  else:
    isEndOfRecord = False
  if line.startswith('template-storage'):
    continue
  if len( row ) == 0:
    row = line.split()
  else:
    row += line.split()
    
  # remove trailing ;
  if isEndOfRecord and row:
    outCsv.writerow( row )
    row = []
   
  
  
