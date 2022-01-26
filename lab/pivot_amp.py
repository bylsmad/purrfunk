#!/usr/bin/env python3

import csv
import argparse
from dataclasses import dataclass
from math import log

LOGTEN=log(10.)

@dataclass
class Row:
  harmonic : int
  trackNumber : int
  freq : float
  amp : float
  newFlag : int

  def __post_init__(self):
    self.harmonic=int(self.harmonic)
    self.trackNumber=int(self.trackNumber)
    self.freq=float(self.freq)
    self.amp=float(self.amp)
    self.newFlag=int(self.newFlag) 

def ampratiotodb(amp1, amp2):
  if amp1 <= 0:
    return -1500
  if amp2 <= 0:
    return 1500
  return 20. / LOGTEN * log (amp1 / amp2)

def rmstodb(amp):
  if amp <= 0:
    return -1500
  return 100. + 20. / LOGTEN * log (amp)

def main(args):
  outFileName=args.inFile.name.replace('oout','hpivot')
  with open(outFileName,'w') as outFile:
    outCsv=csv.writer(outFile,lineterminator='\n')
    inCsv=csv.reader(args.inFile)
    rows=[ Row(*r) for r in inCsv if len(r) == 5 ]
    maxTrack=max([r.trackNumber for r in rows])
    header=['pitch', 'dB'] + list(range(1,args.maxHarmonic + 1)) + ['tweeters','outliers']
    outCsv.writerow(header)
    columnCount=args.maxHarmonic + 2
    newSet=True
    for row in rows:
      if newSet:
        out=[0] * columnCount
        fundamental=None
        newSet=False
      if row.harmonic >= 1 and row.harmonic <= args.maxHarmonic:
        out[row.harmonic - 1]+=row.amp
      elif row.harmonic > args.maxHarmonic:
        out[-2]+=row.amp
      else:
        out[-1]+=row.amp
      if row.harmonic == 1:
        fundamental=row.freq
      if row.trackNumber == maxTrack:
        dbOut=[0] * columnCount
        totalAmp=0
        for harmonic, amp in enumerate(out):
          totalAmp+=row.amp
          if harmonic == 0:
            fundamentalamp=amp
            dbOut[0]=100.
          else:
            dbOut[harmonic]=100. + ampratiotodb(amp,fundamentalamp)
        dB=rmstodb(totalAmp)
            
        outCsv.writerow( [ fundamental, dB ] + dbOut )
        newSet=True
  

if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='transform overtones csv to columns of harmonic')
  parser.add_argument('-i','--inFile', type=argparse.FileType('r'), default=open('bassnote.wav-oout.csv'))
  parser.add_argument('-m','--maxHarmonic', type=int, default=20)
  args = parser.parse_args()
  main(args)
