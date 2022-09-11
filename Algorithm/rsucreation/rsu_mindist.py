import numpy as np
import pandas as pd
from sklearn.cluster import KMeans
from math import hypot

RSU_NUMBER = 75
coords = []
delimiter = " "
with open('rsulist.txt') as f:
    coords = [line.rstrip().split(delimiter) for line in f]

data = np.array(coords)

kmean=KMeans(n_clusters=RSU_NUMBER)
kmean.fit(data)

clust = kmean.labels_

rsugroups = [[] for i in range(RSU_NUMBER)]

for i in range(RSU_NUMBER):
    j=0
    for pos in coords:
        if clust[j] == i:
            rsugroups[i].append(pos)
        j=j+1

finallist = []
for i in range(RSU_NUMBER):
    mindist = 10000
    mean = kmean.cluster_centers_[i]
    selected = None
    for rsu in rsugroups[i]:
        distance = hypot(float(rsu[0])-float(mean[0]), float(rsu[1])-float(mean[1]))
        if distance < mindist:
            selected = rsu
            mindist = distance
    if(selected is not None):
        finallist.append(selected)

with open('clusteredlist.txt', 'w') as output:
    for rsu in finallist:
        print(rsu[0],rsu[1],file=output)
