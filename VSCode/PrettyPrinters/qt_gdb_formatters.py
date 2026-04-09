import gdb
import gdb.printing


class QByteArrayProvider:
    def __init__(self, valueObject):
        self.valueObject = valueObject
    
    def to_string(self):
        pCharString = self.valueObject['d']['ptr'].cast(gdb.lookup_type("char").pointer())
        length = int(self.valueObject['d']['size'].cast(gdb.lookup_type('qsizetype')))
        if int(pCharString.cast(gdb.lookup_type('uint64_t'))) == 0:
            return ''
        if length == 0:
            return '""'
        try:
            return '"' + pCharString.string(encoding = 'latin1', length = length) + '"'
        except Exception:
            return ''
        
    def num_children(self):
        return int(self.valueObject['d']['size'].cast(gdb.lookup_type('qsizetype')))
        
    class _iterator:
        def __init__(self, pData, length):
            self.pData = pData
            self.length = length
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.count >= self.length:
                raise StopIteration
            count = self.count
            self.count = self.count + 1
            return ('[%d]' % count, self.pData[count])

    def children(self):
        pCharString = self.valueObject['d']['ptr'].cast(gdb.lookup_type("char").pointer())
        length = int(self.valueObject['d']['size'].cast(gdb.lookup_type('qsizetype')))
        return self._iterator(pCharString, length)

    def display_hint (self):
        return 'array'
    
class QStringProvider:
    def __init__(self, valueObject):
        self.valueObject = valueObject
    
    def to_string(self):
        pCharString = self.valueObject['d']['ptr'].cast(gdb.lookup_type("char16_t").pointer())
        length = int(self.valueObject['d']['size'].cast(gdb.lookup_type('qsizetype')))
        if int(pCharString.cast(gdb.lookup_type('uint64_t'))) == 0:
            return ''
        if length == 0:
            return '""'
        try:
            return '"' + pCharString.string(encoding = 'UTF-16', length = length) + '"'
        except Exception:
            return ''
        
    def num_children(self):
        return int(self.valueObject['d']['size'].cast(gdb.lookup_type('qsizetype')))
        
    class _iterator:
        def __init__(self, pData, length):
            self.pData = pData
            self.length = length
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.count >= self.length:
                raise StopIteration
            count = self.count
            self.count = self.count + 1
            return ('[%d]' % count, self.pData[count])

    def children(self):
        pCharString = self.valueObject['d']['ptr'].cast(gdb.lookup_type("char16_t").pointer())
        length = int(self.valueObject['d']['size'].cast(gdb.lookup_type('qsizetype')))
        return self._iterator(pCharString, length)

    def display_hint (self):
        return 'array'
    
class QSequentialContainerProvider:
    def __init__(self, valueObject):
        self.valueObject = valueObject

    def to_string(self):
        elementType = self.valueObject.type.template_argument(0)
        pData = self.valueObject['d']['ptr'].cast(elementType.pointer())
        length = int(self.valueObject['d']['size'].cast(gdb.lookup_type('qsizetype')))
        if int(pData.cast(gdb.lookup_type('uint64_t'))) == 0:
            return ''
        if length == 0:
            return '""'
        try:
            typeName = f"{self.valueObject.type}"
            return typeName + ", size=" + str(length)
        except Exception:
            return ''
        
    def num_children(self):
        return int(self.valueObject['d']['size'].cast(gdb.lookup_type('qsizetype')))
        
    class _iterator:
        def __init__(self, pData, length):
            self.pData = pData
            self.length = length
            self.count = 0

        def __iter__(self):
            return self

        def __next__(self):
            if self.count >= self.length:
                raise StopIteration
            count = self.count
            self.count = self.count + 1
            return ('[%d]' % count, self.pData[count])

    def children(self):
        elementType = self.valueObject.type.template_argument(0)
        pData = self.valueObject['d']['ptr'].cast(elementType.pointer())
        length = int(self.valueObject['d']['size'].cast(gdb.lookup_type('qsizetype')))
        return self._iterator(pData, length)

    def display_hint (self):
        return 'array'

class QMapProvider:
    def __init__(self, valobj):
        self.valobj = valobj

    def __get_std_map(self):
        ptr = self.valobj['d']['d']['ptr']
        if int(ptr.cast(gdb.lookup_type('uint64_t'))) != 0:
            map = ptr['m']
            mapType = map.type.strip_typedefs()
            return gdb.default_visualizer(map.cast(mapType))
        else:
            return None

    def to_string(self):
        map = self.__get_std_map()
        typeName = f"{self.valobj.type}"
        if map != None:
            try:
                mapIt = map.children()
                size = int((sum(1 for _ in mapIt))/2)
                return typeName + ", size=" + str(size)
            except Exception:
                return typeName
        else:
           return typeName
        
    def children(self):
        try:
            return self.__get_std_map().children()
        except Exception:
            return None
        
    def display_hint (self):
        return 'map'
    
# Formatter below is based on qdumpHelper_QHash_6 from 
# https://github.com/qt-creator/qt-creator/blob/master/share/qtcreator/debugger/qttypes.py
class QHashProvider:
    def __init__(self, valobj):
        self.valobj = valobj

    def update(self):
        self.size = 0
        self.nodes = list()
        pointer = self.valobj.GetChildMemberWithName('d')
        if pointer.GetValueAsUnsigned() != 0:
            self.size = self.valobj.GetChildMemberWithName('d').GetChildMemberWithName('size').GetValueAsUnsigned()
            self.nodes = [lldb.SBValue]*self.size
            bucketCount = self.valobj.GetChildMemberWithName('d').GetChildMemberWithName('numBuckets').GetValueAsUnsigned()
            pSpans = self.valobj.GetChildMemberWithName('d').GetChildMemberWithName('spans')
            pointerSize = pSpans.GetType().GetCanonicalType().GetByteSize()
            spanType = pSpans.GetType().GetPointeeType().GetCanonicalType()
            nodeType = spanType.GetTemplateArgumentType(0)
            nodeSize = nodeType.GetByteSize()
            pSpansAddress = pSpans.GetValueAsUnsigned()
            spanSizeInBytes = 128 + 2 * pointerSize
            spanCount = int((bucketCount + 127) / 128)
            count = 0
            for b in range(spanCount):
                pCurrentSpanAddress = pSpansAddress + b * spanSizeInBytes
                currentSpan = self.valobj.CreateValueFromAddress("", pCurrentSpanAddress, spanType)
                offsets = currentSpan.GetChildMemberWithName('offsets')
                pEntries = currentSpan.GetChildMemberWithName('entries')
                pEntriesAddress = pEntries.GetValueAsUnsigned()
                for i in range(128):
                    offset = offsets.GetChildAtIndex(i).GetValueAsUnsigned()
                    if offset != 255:
                        pCurrentEntryAddress = pEntriesAddress + offset * nodeSize
                        node = self.valobj.CreateValueFromAddress("", pCurrentEntryAddress, nodeType)
                        self.nodes[count] = node
                        count += 1

    def num_children(self):
        return self.size
    
    def get_child_at_index(self,index):
        if 0 <= index and index < self.num_children():
            return self.nodes[index]
        else:
            return None
    
    def has_children(self):
        return self.num_children() > 0
    
    @staticmethod
    def provide_summary(valobj, internal_dict, options=None):
        return valobj.GetTypeName() + ", size=" + str(valobj.GetNumChildren())


def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("qt_gdb_formatters")
    pp.add_printer('QByteArray', '^QByteArray$', QByteArrayProvider)
    pp.add_printer('QString', '^QString$', QStringProvider)
    pp.add_printer('QList', '^QList<.*>$', QSequentialContainerProvider)
    pp.add_printer('QVector', '^QVector<.*>$', QSequentialContainerProvider)
    pp.add_printer('QQueue', '^QQueue<.*>$', QSequentialContainerProvider)
    pp.add_printer('QStack', '^QStack<.*>$', QSequentialContainerProvider)
    pp.add_printer('QMap', '^QMap<.*>$', QMapProvider)
    pp.add_printer('QHash', '^QHash<.*>$', QHashProvider)
    return pp

gdb.printing.register_pretty_printer(
    gdb.current_objfile(),
    build_pretty_printer())
