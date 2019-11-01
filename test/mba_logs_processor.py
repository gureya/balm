import math
import os

number_mba = 8;
number_ratios = 8;
mba_table = [[0] * number_mba for i in range(number_ratios)];
i = 0;
j = 0;

sum_ipc = 0;
plr = 100;
mba_level = 10;
mba_index = 0;
plr_index = 0;

start_index = 0;

with open("ocean_cp_colocation.txt") as f:
    for line in f:
        if line.startswith("Page"):
            plr = line.strip().split(" ")[3]
            plr = int(plr)
            print "Page Locality Ratio", plr
            print "MBA Level (%)\t IPC"
            mba_index = 0;
            if (start_index == 0):
                plr_index = 0;
                start_index = 1;
            else:
                plr_index += 1;
        elif line.startswith("MBA"):
            mba_level = line.strip().split(" ")[2]
            mba_level = int(mba_level)
            #print "MBA Level", mba_level
        elif line.startswith("2019"):
            ipc = line.strip().split(",")[2]
            ipc = float(ipc)
            if (i != 0):
                sum_ipc += ipc;
            i += 1;
            j += 1;
            #print ipc, sum_ipc
            if ( i == 4):
                avg_ipc = sum_ipc/3;
                #print "avg_ipc: ", '{0:.2f}'.format(avg_ipc)
                print mba_level, "\t", '{0:.2f}'.format(avg_ipc)

                mba_table[mba_index][plr_index] = '{0:.2f}'.format(avg_ipc)

                i = 0;
                sum_ipc = 0;
                mba_index += 1;
            #if (j == 40):
            #   break
print "\t100\t90\t60\t50\t40\t30\t20\t10"
x = 10;
#for row in mba_table:
   # print x, row
    #if (x == 60):
    #    x += 30;
    #else:
    #    x += 10;

for i in range(number_mba):
    print x, "\t",
    if (x == 60):
        x += 30;
    else:
        x += 10;
    for j in range(number_ratios):
        print mba_table[i][j], "\t",
    print('\n')
