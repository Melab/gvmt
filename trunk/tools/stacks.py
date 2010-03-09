import itertools, re

#Prototype stack
class Stack(object):
    
    def push(self, value, out):
        raise NotImplementedError
        
    def pop(self, out):
        raise NotImplementedError
    
    def push_double(self, value, out):
        raise NotImplementedError
        
    def pop_double(self, out):
        raise NotImplementedError
        
    def flush_cache(self, out):
        raise NotImplementedError
        
    def flush_to_memory(self, out, retain = 0):
        raise NotImplementedError
        
    def insert(self, offset, size, out):
        raise NotImplementedError
        
    def drop(self, offset, size, out):
        raise NotImplementedError 
        
    def store(self, out):
        raise NotImplementedError 
        
    def pick(self, index, out):
        raise NotImplementedError 
        
    def top(self, out):
        raise NotImplementedError 
        
    def join(self, out):
        raise NotImplementedError
        
    def transform(self, other, transformer, out):
        raise NotImplementedError
        
    def copy(self):
        raise NotImplementedError
        
class CachingStack(Stack):
        
    def __init__(self, backing):
        self.backing = backing
        self.cache = []
        self.item_size = []
        self._cache_size = 0
        self.declarations = backing.declarations
        
    def push_double(self, value, out):
        value = value.store(self.declarations, out)
        self.cache.append(value)
        self.item_size.append(2)
        self._cache_size += 2
        
    def push(self, value, out):
        self.cache.append(value)
        self.item_size.append(1)
        self._cache_size += 1
        
    def pop_double(self, out):
        if self.item_size and self.item_size[-1] == 2:
            self.item_size.pop()
            self._cache_size -= 2
            return self.cache.pop()
        else:
            self.flush_cache(out, 0)
            return self.backing.pop_double(out)
            
    def pop(self, out):
        if self.item_size and self.item_size[-1] == 1:
            self.item_size.pop()
            self._cache_size -= 1
            return self.cache.pop()
        else:
            self.flush_cache(out, 0)
            return self.backing.pop(out)
            
    def pick(self, index, out):
        self.flush_cache(out, 0)
        return self.backing.pick(index, out)
        
    def flush_cache(self, out, retain = 0):
        while self._cache_size > retain:
            self._push_back(out)
        while self._cache_size < retain:
            self._pull_up(out)
        
    def flush_to_memory(self, out, retain = 0):
        self.flush_cache(out, retain)
        self.backing.flush_to_memory(out)
        
    def join(self, out):
        self.flush_cache(out)
        self.backing.join(out)
        
    def top(self, out):
        return self.backing.top(out, self._cache_size)
            
    def _push_back(self, out):
        c = self.cache.pop(0)
        size = self.item_size.pop(0)
        self._cache_size -= size
        if size == 2:
            self.backing.push_double(c, out)
        else:    
            self.backing.push(c, out)
        
    def _pull_up(self, out):   
        self.cache.insert(0, self.backing.pop(out))
        self._cache_size += 1
        self.item_size.insert(0, 1)
        
    def insert(self, offset, size, out):
        #Is offset a build-time constant?
        try:
            offset = int(offset)
            while self._cache_size > offset:
                self._push_back(out)
            while self._cache_size < offset:
                self._pull_up(out)
            return self.backing.insert(0, size, out)
        except ValueError:
            self.flush_cache(out)
            return self.backing.insert(offset, size, out)
        
    def drop(self, offset, size, out):
        try:
            offset = int(offset)
            while self._cache_size > offset:
                self._push_back(out)
            while self._cache_size < offset:
                self._pull_up(out)
            self.backing.drop(0, size, out)
        except ValueError:
            self.flush_cache(out)
            self.backing.drop(offset, size, out)
                
    def comment(self, out):
        out << '// Cache: '
        for i in self.cache:
            out << str(i) << ' '
        out << '\n'
        self.backing.comment(out)
    
    def transform(self, other, transformer, out):
        while self._cache_size > other._cache_size:
            self._push_back(out)
        while self._cache_size < other._cache_size:
            self._pull_up(out)
        assert self._cache_size == other._cache_size
        self.backing.transform(other.backing, transformer, out)
        for s, o in itertools.izip(self.cache, other.cache):
            transformer(s, o)
        
    def store(self, out):
        for i in range(len(self.cache)):
            self.cache[i] = self.cache[i].store(self.declarations, out)
                
    def copy(self):
        result = self.__class__(self.backing.copy())
        result.cache = self.cache[:]
        result.item_size = self.item_size[:]
        result._cache_size = self._cache_size
        return result

    
