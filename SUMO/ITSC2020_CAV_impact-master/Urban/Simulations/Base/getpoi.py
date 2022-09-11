import os, sys

if 'SUMO_HOME' in os.environ:
    tools = os.path.join(os.environ['SUMO_HOME'], 'tools')
    sys.path.append(tools)
else:
    sys.exit("please declare environment variable 'SUMO_HOME'")

import traci

sumoBinary = "/usr/bin/sumo"
sumoCmd = [sumoBinary, "-c", "DCC_simulation.sumo.cfg"]

traci.start(sumoCmd)

traci.simulationStep(0)

for poi in traci.poi.getIDList():
	print(traci.poi.getPosition(poi))

traci.close()
#for poi in traci.domain.Domain.getIDList(self):
#	print(getPosition(self, poi).first, getPosition(self, poi).second)
