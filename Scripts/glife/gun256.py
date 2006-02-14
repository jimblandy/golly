from glife.herschel import *

__c64 = hc64 (9, 0)
__stub = eater (4, -16, rccw)

gun256_full = herschel (9, 0) + __c64 + __c64.apply (rcw) + __c64.apply (flip) + __c64.apply (rccw)
gun256      = (gun256_full + __stub + __stub.apply (rccw) + __stub.apply (flip)) (-25, -12)
