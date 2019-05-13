import sys,os
import autosar

from BSWCOM import *

class InactiveActive_T(autosar.Template):
   valueTable=['InactiveActive_Inactive',
               'InactiveActive_Active',
               'InactiveActive_Error',
               'InactiveActive_NotAvailable']
   @classmethod
   def apply(cls, ws):
      package = ws.getDataTypePackage()
      if package.find(cls.__name__) is None:
         package.createIntegerDataType(cls.__name__, valueTable=cls.valueTable)

class OnOff_T(autosar.Template):
   valueTable=['OnOff_Off',
               'OnOff_On',
               'OnOff_1Hz',
               'OnOff_2Hz',
               'OnOff_3Hz']
   @classmethod
   def apply(cls, ws):
      package = ws.getDataTypePackage()
      if package.find(cls.__name__) is None:
         package.createIntegerDataType(cls.__name__, valueTable=cls.valueTable)

ttList=['TPMS','LowOil','PosLamp','TurnLeft','TurnRight','AutoCruise','HighBeam',
                'SeatbeltDriver','SeatbeltPassenger','Airbag']
# TelltaleStatus: means the COM signals which control the related Telltale status
# TelltaleState: means the state of Telltale: on, off or flash
TELLTALE_D = []
for tt in ttList:
    TELLTALE_D.append(autosar.createDataElementTemplate('%sState'%(tt), OnOff_T))
TELLTALE_I = autosar.createSenderReceiverInterfaceTemplate('TELLTALE_I', TELLTALE_D)
TELLTALE_P = autosar.createSenderReceiverPortTemplate('Telltale', TELLTALE_I)

class Telltale(autosar.Template):
    @classmethod
    def apply(cls, ws):
        componentName = cls.__name__
        package = ws.getComponentTypePackage()
        if package.find(componentName) is None:
            swc = package.createApplicationSoftwareComponent(componentName)
            cls.addPorts(swc)
            cls.addBehavior(swc)

    @classmethod
    def addPorts(cls, swc):
        componentName = cls.__name__
        swc.apply(COM_P.Receive)
        swc.apply(TELLTALE_P.Send)

    @classmethod
    def addBehavior(cls, swc):
        portAccess=['Telltale/%sState'%(x) for x in ttList]
        portAccess += [Led1Sts, Led2Sts, Led3Sts]
        swc.behavior.createRunnable('Telltale_run', portAccess=portAccess)
        swc.behavior.createTimingEvent('Telltale_run', period=20)

if(__name__ == '__main__'):
    autosar.asSWCGen(Telltale)