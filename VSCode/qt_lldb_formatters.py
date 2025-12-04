import lldb


class QByteArrayProvider:
    def __init__(self, valueObject, internal_dict):
        self.valueObject = valueObject
        self.update()

    def update(self):
        self.size = self.valueObject.GetChildMemberWithName("d").GetChildMemberWithName("size").GetValueAsSigned()
        if self.size > 0:
            self.data = self.valueObject.GetChildMemberWithName("d").GetChildMemberWithName("ptr")
        else:
            self.data = None
        return True

    def num_children(self):
        return self.size

    def get_child_at_index(self, index):
        if 0 <= index and index < self.num_children():
            element_type = self.data.GetType().GetPointeeType()
            element_size = element_type.GetByteSize()
            offset = index * element_size
            child_address = self.data.GetValueAsUnsigned() + offset
            return self.valueObject.CreateValueFromAddress(f"[{index}]", child_address, element_type)
        else:
            return None

    def has_children(self):
        return self.num_children() > 0
    
    @staticmethod
    def provide_summary(valobj, internal_dict, options=None):
        valobj.SetPreferSyntheticValue(False)
        pointer = valobj.GetChildMemberWithName("d").GetChildMemberWithName("ptr").GetValueAsUnsigned()
        length = valobj.GetChildMemberWithName("d").GetChildMemberWithName("size").GetValueAsSigned()
        if pointer == 0:
            return ''
        if length == 0:
            return '""'
        error = lldb.SBError()
        string_data = valobj.process.ReadMemory(pointer, length, error)
        if error.Success():
            return string_data
        else:
            return ''
    

class QStringProvider:
    def __init__(self, valueObject, internal_dict):
        self.valueObject = valueObject
        self.update()

    def update(self):
        self.size = self.valueObject.GetChildMemberWithName("d").GetChildMemberWithName("size").GetValueAsSigned()
        if self.size > 0:
            self.data = self.valueObject.GetChildMemberWithName("d").GetChildMemberWithName("ptr")
        else:
            self.data = None
            self.size = 0
        return True

    def num_children(self):
        return self.size

    def get_child_at_index(self, index):
        if 0 <= index and index < self.num_children():
            element_type = self.data.GetType().GetPointeeType()
            element_size = element_type.GetByteSize()
            offset = index * element_size
            child_address = self.data.GetValueAsUnsigned() + offset
            return self.valueObject.CreateValueFromAddress(f"[{index}]", child_address, element_type)
        else:
            return None

    def has_children(self):
        return self.num_children() > 0
    
    @staticmethod
    def provide_summary(valobj, internal_dict, options=None):
        valobj.SetPreferSyntheticValue(False)
        pointer = valobj.GetChildMemberWithName("d").GetChildMemberWithName("ptr").GetValueAsUnsigned()
        length = 2*valobj.GetChildMemberWithName("d").GetChildMemberWithName("size").GetValueAsSigned()
        if pointer == 0:
            return ''
        if length == 0:
            return '""'
        error = lldb.SBError()
        string_data = valobj.process.ReadMemory(pointer, length, error)
        if error.Success():
            return string_data.decode("utf-16").encode("utf-8")
        else:
            return ''
    

class QSequentialContainerProvider:
    def __init__(self, valueObject, internal_dict):
        self.valueObject = valueObject
        self.update()

    def update(self):
        self.size = self.valueObject.GetChildMemberWithName("d").GetChildMemberWithName("size").GetValueAsSigned()
        if self.size > 0:
            self.data = self.valueObject.GetChildMemberWithName("d").GetChildMemberWithName("ptr")
        else:
            self.data = None
        return True

    def num_children(self):
        return self.size

    def get_child_at_index(self, index):
        if 0 <= index and index < self.num_children():
            element_type = self.data.GetType().GetPointeeType()
            element_size = element_type.GetByteSize()
            offset = index * element_size
            child_address = self.data.GetValueAsUnsigned() + offset
            return self.valueObject.CreateValueFromAddress(f"[{index}]", child_address, element_type)
        else:
            return None

    def has_children(self):
        return self.num_children() > 0
    
    @staticmethod
    def provide_summary(valobj, internal_dict, options=None):
        return valobj.GetTypeName() + ", size=" + str(valobj.GetNumChildren())
    

class QMapProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        ptr = self.valobj.GetChildMemberWithName('d').GetChildMemberWithName('d').GetChildMemberWithName('ptr')
        if ptr.GetValueAsUnsigned() != 0:
            map = ptr.GetChildMemberWithName('m')
            mapType = map.GetType().GetCanonicalType()
            self.wrapped_member = map.Cast(mapType)
            self.wrapped_member.SetPreferSyntheticValue(True)
        else:
            self.wrapped_member = None

    def num_children(self):
        if self.wrapped_member != None:
            return self.wrapped_member.GetNumChildren()
        else:
            return 0
    
    def get_child_index(self,name):
        if self.wrapped_member != None:
            return self.wrapped_member.GetChildIndex(name)
        else:
            return None
    
    def get_child_at_index(self,index):
        if self.wrapped_member != None:
            return self.wrapped_member.GetChildAtIndex(index)
        else:
            return None
        
    def has_children(self):
        return self.num_children() > 0
    
    @staticmethod
    def provide_summary(valobj, internal_dict, options=None):
        return valobj.GetTypeName() + ", size=" + str(valobj.GetNumChildren())
    

# Formatter below is based on qdumpHelper_QHash_6 from 
# https://github.com/qt-creator/qt-creator/blob/master/share/qtcreator/debugger/qttypes.py
class QHashProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

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
    

# Formatter below is based on qdumpHelper_QSet6 from 
# https://github.com/qt-creator/qt-creator/blob/master/share/qtcreator/debugger/qttypes.py
class QSetProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        hash = self.valobj.GetChildMemberWithName('q_hash')
        hashType = hash.GetType().GetCanonicalType()
        self.wrapped_member = hash.Cast(hashType)
        self.wrapped_member.SetPreferSyntheticValue(True)

    def num_children(self):
        return self.wrapped_member.GetNumChildren()
    
    def get_child_at_index(self,index):
        return self.wrapped_member.GetChildAtIndex(index)
        
    def has_children(self):
        return self.num_children() > 0
    
    @staticmethod
    def provide_summary(valobj, internal_dict, options=None):
        return valobj.GetTypeName() + ", size=" + str(valobj.GetNumChildren())
    

# Formatter below is based on qdumpHelper_QMultiHash_6 from 
# https://github.com/qt-creator/qt-creator/blob/master/share/qtcreator/debugger/qttypes.py
class QMultiHashProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        self.size = max(0, self.valobj.GetChildMemberWithName('m_size').GetValueAsSigned())
        self.nodes = list()
        pointer = self.valobj.GetChildMemberWithName('d')
        if pointer.GetValueAsUnsigned() != 0:
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
    

class QHashPrivateMultiNodeChainProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        self.values = list()
        pCurrentNode = self.valobj
        while True:
            self.values.append(pCurrentNode.GetChildMemberWithName("value"))
            pCurrentNode = pCurrentNode.GetChildMemberWithName('next')
            if pCurrentNode.GetValueAsUnsigned() == 0:
                break

    def num_children(self):
        return len(self.values)
    
    def get_child_at_index(self,index):
        if 0 <= index and index < self.num_children():
            return self.values[index]
        else:
            return None
    
    def has_children(self):
        return self.num_children() > 0
    
    @staticmethod
    def provide_summary(valobj, internal_dict, options=None):
        return valobj.GetTypeName() + ", size=" + str(valobj.GetNumChildren())
    

class QVariantProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        self.size = 0
        self.value = None
        typeName = QVariantProvider.__getTypeName(self.valobj)
        if len(typeName) == 0:
            return
        self.size = 1
        isShared = self.valobj.GetChildMemberWithName('d').GetChildMemberWithName('is_shared').GetValueAsUnsigned()
        if isShared == 0:
            pValue = self.valobj.GetChildMemberWithName('d').GetChildMemberWithName('data').GetChildMemberWithName('data').GetLoadAddress()
        else:
            shared = self.valobj.GetChildMemberWithName('d').GetChildMemberWithName('data').GetChildMemberWithName('shared')
            pShared = shared.GetLoadAddress()
            offset = shared.GetChildMemberWithName('offset').GetValueAsUnsigned()
            pValue = pShared + offset
        valueType = self.valobj.GetTarget().FindFirstType(typeName)
        self.value = self.valobj.CreateValueFromAddress("value", pValue, valueType)

    def num_children(self):
        return self.size
    
    def get_child_at_index(self,index):
        if 0 <= index and index < self.num_children():
            return self.value
        else:
            return None
    
    def has_children(self):
        return self.num_children() > 0
    
    @staticmethod
    def __getTypeName(valobj):
        pMetaTypeInterface = 4 * valobj.GetChildMemberWithName('d').GetChildMemberWithName('packedType').GetValueAsUnsigned()
        typeName = ""
        if pMetaTypeInterface != 0:
            metaTypeInterfaceType = valobj.GetTarget().FindFirstType("QtPrivate::QMetaTypeInterface")
            metaTypeInterface = valobj.CreateValueFromAddress("", pMetaTypeInterface, metaTypeInterfaceType)
            pTypeName = metaTypeInterface.GetChildMemberWithName('name').GetValueAsUnsigned()
            if pTypeName != 0:
                error = lldb.SBError()
                typeName = valobj.process.ReadCStringFromMemory(pTypeName, 1024, error)
                if not error.Success():
                    typeName = ""
        return typeName
    
    @staticmethod
    def provide_summary(valobj, internal_dict, options=None):
        valobj.SetPreferSyntheticValue(False)
        typeName = QVariantProvider.__getTypeName(valobj)
        if len(typeName) == 0:
            typeName = "invalid"
        return "QVariant (" + typeName + ")"
    

class QUrlProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        self.values = list()
        pUrlPrivate = self.valobj.GetChildMemberWithName('d').GetValueAsUnsigned()
        if pUrlPrivate != 0:
            intType = self.valobj.GetTarget().FindFirstType("int")
            qStringType = self.valobj.GetTarget().FindFirstType("QString")
            address = pUrlPrivate + 4
            self.values.append(self.valobj.CreateValueFromAddress("port", address, intType))
            address += 4
            self.values.append(self.valobj.CreateValueFromAddress("scheme", address, qStringType))
            address += qStringType.GetByteSize()
            self.values.append(self.valobj.CreateValueFromAddress("userName", address, qStringType))
            address += qStringType.GetByteSize()
            self.values.append(self.valobj.CreateValueFromAddress("password", address, qStringType))
            address += qStringType.GetByteSize()
            self.values.append(self.valobj.CreateValueFromAddress("host", address, qStringType))
            address += qStringType.GetByteSize()
            self.values.append(self.valobj.CreateValueFromAddress("path", address, qStringType))
            address += qStringType.GetByteSize()
            self.values.append(self.valobj.CreateValueFromAddress("query", address, qStringType))
            address += qStringType.GetByteSize()
            self.values.append(self.valobj.CreateValueFromAddress("fragment", address, qStringType))

    def num_children(self):
        return len(self.values)
    
    def get_child_at_index(self,index):
        if 0 <= index and index < self.num_children():
            return self.values[index]
        else:
            return None
    
    def has_children(self):
        return self.num_children() > 0
    
    @staticmethod
    def provide_summary(valobj, internal_dict, options=None):
        return valobj.GetTypeName()


def __lldb_init_module(debugger, dict):
    debugger.HandleCommand('type summary add QByteArray -F qt_lldb_formatters.QByteArrayProvider.provide_summary')
    debugger.HandleCommand('type synthetic add QByteArray --python-class qt_lldb_formatters.QByteArrayProvider')
    debugger.HandleCommand('type summary add QString -F qt_lldb_formatters.QStringProvider.provide_summary')
    debugger.HandleCommand('type synthetic add QString --python-class qt_lldb_formatters.QStringProvider')
    debugger.HandleCommand('type summary add -x "QList<" -F qt_lldb_formatters.QSequentialContainerProvider.provide_summary')
    debugger.HandleCommand('type synthetic add -x "QList<" --python-class qt_lldb_formatters.QSequentialContainerProvider')
    debugger.HandleCommand('type summary add QByteArrayList -F qt_lldb_formatters.QSequentialContainerProvider.provide_summary')
    debugger.HandleCommand('type synthetic add QByteArrayList --python-class qt_lldb_formatters.QSequentialContainerProvider')
    debugger.HandleCommand('type summary add QStringList -F qt_lldb_formatters.QSequentialContainerProvider.provide_summary')
    debugger.HandleCommand('type synthetic add QStringList --python-class qt_lldb_formatters.QSequentialContainerProvider')
    debugger.HandleCommand('type summary add -x "QVector<" -F qt_lldb_formatters.QSequentialContainerProvider.provide_summary')
    debugger.HandleCommand('type synthetic add -x "QVector<" --python-class qt_lldb_formatters.QSequentialContainerProvider')
    debugger.HandleCommand('type summary add -x "QQueue<" -F qt_lldb_formatters.QSequentialContainerProvider.provide_summary')
    debugger.HandleCommand('type synthetic add -x "QQueue<" --python-class qt_lldb_formatters.QSequentialContainerProvider')
    debugger.HandleCommand('type summary add -x "QStack<" -F qt_lldb_formatters.QSequentialContainerProvider.provide_summary')
    debugger.HandleCommand('type synthetic add -x "QStack<" --python-class qt_lldb_formatters.QSequentialContainerProvider')
    debugger.HandleCommand('type summary add -x "QMap<" -F qt_lldb_formatters.QMapProvider.provide_summary')
    debugger.HandleCommand('type synthetic add -x "QMap<" --python-class qt_lldb_formatters.QMapProvider')
    debugger.HandleCommand('type summary add -x "QHash<" -F qt_lldb_formatters.QHashProvider.provide_summary')
    debugger.HandleCommand('type synthetic add -x "QHash<" --python-class qt_lldb_formatters.QHashProvider')
    debugger.HandleCommand('type summary add -x "QSet<" -F qt_lldb_formatters.QSetProvider.provide_summary')
    debugger.HandleCommand('type synthetic add -x "QSet<" --python-class qt_lldb_formatters.QSetProvider')
    debugger.HandleCommand('type summary add -x "QMultiHash<" -F qt_lldb_formatters.QMultiHashProvider.provide_summary')
    debugger.HandleCommand('type synthetic add -x "QMultiHash<" --python-class qt_lldb_formatters.QMultiHashProvider')
    debugger.HandleCommand('type summary add -x "QHashPrivate::MultiNodeChain<" -F qt_lldb_formatters.QHashPrivateMultiNodeChainProvider.provide_summary')
    debugger.HandleCommand('type synthetic add -x "QHashPrivate::MultiNodeChain<" --python-class qt_lldb_formatters.QHashPrivateMultiNodeChainProvider')
    debugger.HandleCommand('type summary add "QVariant" -F qt_lldb_formatters.QVariantProvider.provide_summary')
    debugger.HandleCommand('type synthetic add "QVariant" --python-class qt_lldb_formatters.QVariantProvider')
    debugger.HandleCommand('type summary add -x "QVariantList" -F qt_lldb_formatters.QSequentialContainerProvider.provide_summary')
    debugger.HandleCommand('type synthetic add -x "QVariantList" --python-class qt_lldb_formatters.QSequentialContainerProvider')
    debugger.HandleCommand('type summary add "QVariantMap" -F qt_lldb_formatters.QMapProvider.provide_summary')
    debugger.HandleCommand('type synthetic add "QVariantMap" --python-class qt_lldb_formatters.QMapProvider')
    debugger.HandleCommand('type summary add "QVariantHash" -F qt_lldb_formatters.QHashProvider.provide_summary')
    debugger.HandleCommand('type synthetic add "QVariantHash" --python-class qt_lldb_formatters.QHashProvider')
    debugger.HandleCommand('type summary add "QUrl" -F qt_lldb_formatters.QUrlProvider.provide_summary')
    debugger.HandleCommand('type synthetic add "QUrl" --python-class qt_lldb_formatters.QUrlProvider')
    debugger.HandleCommand("settings set target.prefer-dynamic-value no-dynamic-values")
    debugger.HandleCommand("settings set target.process.follow-fork-mode parent")
    debugger.HandleCommand("settings set symbols.load-on-demand true")
    print("Loaded Qt formatters")
