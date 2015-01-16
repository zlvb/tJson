#include <vector>
#include <string>
#include <map>
#include <malloc.h>
#include <assert.h>
namespace tjson
{
    class Value;
    size_t parse(const char *s, size_t len, Value *root);

    enum Type
    {
        JT_NULL,
        JT_ARRAY,
        JT_BOOL,
        JT_FLOAT,
        JT_INTEGER,        
        JT_OBJECT,
        JT_STRING,
    };

    namespace internal
    {
        struct MapData;
        struct StringData;
        struct VectorData;
        class Map;
        class String;
        class Vector;

        void *jsmalloc(size_t s);
        void jsfree(void *p, size_t s);

        template <class T>
        class jmem_obj
        {
        public:
            virtual ~jmem_obj(){};
            void *operator new(size_t s)
            {
                return jsmalloc(s);
            }
            void *operator new[](size_t s)
            {
                char *p = (char*)jsmalloc(s + sizeof(size_t));
                *(size_t*)p = s/sizeof(T);
                return p + sizeof(size_t);
            }
            void operator delete(void *p)
            {
                jsfree(p, sizeof(T));
            }
            void operator delete[](void *p)
            {
                size_t c = *(size_t*)((char*)p-sizeof(size_t));
                jsfree((char*)p-sizeof(size_t), c*sizeof(T));
            }
        };

        template<typename _Tp>
        class jmem_alloc
        {
        public:
            typedef size_t     size_type;
            typedef ptrdiff_t  difference_type;
            typedef _Tp*       pointer;
            typedef const _Tp* const_pointer;
            typedef _Tp&       reference;
            typedef const _Tp& const_reference;
            typedef _Tp        value_type;

            template<typename _Tp1>
            struct rebind
            {
                typedef jmem_alloc<_Tp1> other;
            };

            jmem_alloc() { }

            jmem_alloc(const jmem_alloc&) { }

            template<typename _Tp1>
            jmem_alloc(const jmem_alloc<_Tp1>&) { }

            ~jmem_alloc() { }

            pointer address(reference __x) const
            {
                return &__x;
            }

            const_pointer address(const_reference __x) const
            {
                return &__x;
            }

            pointer allocate(size_type __n, const void* = 0)
            {
                if( __n <= 0 )
                    return NULL;
                if (__n > this->max_size())
                {
                    throw std::bad_alloc();
                }

                _Tp* p = NULL;
                p = (_Tp*)jsmalloc(__n * sizeof(_Tp));
                pointer rp(p);
                return rp;
            }

            void deallocate(pointer __p, size_type c)
            {
                jsfree(__p, c * sizeof(_Tp));
            }

            size_type max_size() const
            {
                return size_t(-1) / sizeof(_Tp);
            }

            void construct(pointer __p, const _Tp& __val)
            {
                ::new(reinterpret_cast<void*>(__p)) _Tp(__val);
            }

            void destroy(pointer __p)
            {
                __p->~_Tp();
            }
        };

        template <class T>
        inline void delete_data(T *pData)
        {
            if (pData)
            {
                pData->ref--;
                if (pData->ref <= 0)
                {
                    delete pData;
                    pData = NULL;
                }
            }
        }

        template <class T>
        inline size_t data_size(const T *pData)
        {
            if (pData)
            {
                return pData->size();
            }
            return 0;
        }

        template <class T>
        inline void data_copy(T &l, const T &r)
        {
            l.m_data = r.m_data;
            if (l.m_data)
            {
                l.m_data->ref++;
            }
        }

        template <class T>
        inline T& data_set(T &l, const T &r)
        {
            if (l.m_data != r.m_data)
            {
                delete_data(l.m_data);
                data_copy(l, r);
            }
            return l;
        }

        struct StringData : public jmem_obj<StringData>
        {
            StringData();
            ~StringData();
            size_t size() const {return value_size;}
            StringData &operator=(const char *s);
            void assign(const char *s, size_t len);
            int ref;
            char *buff;
        private:            
            size_t value_size;
            size_t buff_capacity;            
        };    

        class String : public jmem_obj<String>
        {
        public:
            String():m_data(NULL){}

            String(const char *s, size_t c)
            {
                m_data = new StringData;
                m_data->assign(s, c);
            }
            String(const String &s)
            {
                data_copy(*this, s);
            }
            ~String()
            {
                delete_data(m_data);
            }
            bool operator == (const String &s) const;
            String &operator=(const String &s)
            {
                return data_set(*this, s);
            }
            size_t size() const
            {
                return data_size(m_data);
            }        

            bool operator<(const String &r) const;

            const char *c_str() const
            {
                if (m_data)
                {
                    return m_data->buff;
                }
                return NULL;
            }
            template <class T>
            friend void data_copy(T &l, const T &r);
            template <class T>
            friend T& data_set(T &l, const T &r);
        private:
            StringData *m_data;
        };

        struct VectorData : public jmem_obj<VectorData>
        {            
            VectorData();
            ~VectorData();
            void increase_size();
            size_t size() const {return value_size;}  
            int ref;
            Value *buff;
        private:            
            size_t value_size;
            size_t buff_capacity;            
        };

        class Vector : public jmem_obj<Vector>
        {
        public:
            Vector():m_data(NULL){}
            Vector(const Vector &vec)
            {
                data_copy(*this, vec);
            }
            ~Vector()
            {
                delete_data(m_data);
            }
            Vector &operator=(const Vector &s)
            {
                return data_set(*this, s);
            }
            Value *back();
            Value *push_back();
            size_t size() const
            {
                return data_size(m_data);
            } 
            Value &operator[](size_t idx);
        private:
            VectorData *m_data;
            template <class T>
            friend void data_copy(T &l, const T &r);
            template <class T>
            friend T& data_set(T &l, const T &r);
        };
          
    }// namespace internal

    class Value : public internal::jmem_alloc<Value>
    {
    public:
        Value()
            :m_type(JT_NULL),m_intval(0),internal_parent(NULL)
        {

        }
        Value(const Value &v);
        ~Value();
        Value &operator[](size_t index)
        {
            if (m_type == JT_ARRAY)
            {
                return (*m_array)[index];
            }
            return Null;
        }
        const Value &operator[](size_t index) const
        {
            if (m_type == JT_ARRAY)
            {
                return (*m_array)[index];
            }
            return Null;
        }
        const Value &operator[](const char *k) const;
        bool IsBool() const    { return m_type == JT_BOOL;    }
        bool IsFloat() const   { return m_type == JT_FLOAT;   }
        bool IsInteger() const { return m_type == JT_INTEGER; }
        bool IsString() const  { return m_type == JT_STRING;  }
        bool IsArray() const   { return m_type == JT_ARRAY;   }
        bool IsObject() const  { return m_type == JT_OBJECT;  }
        bool IsNull() const    { return m_type == JT_NULL;    }
        Type GetType() const   { return m_type;               }
        bool AsBool() const         { return m_bool;   }
        double AsFloat() const      { return m_fval;   }
        long long AsInteger() const { return m_intval; }
        const char* AsString() const 
        { 
            if (m_type == JT_STRING)
            {
                assert(m_strval);
                return m_strval->c_str(); 
            }
            return NULL;
        }
        static Value Null;
        
        size_t Size() const;
        template <class VECT>
        void GetKeys(VECT *vec) const;
    private:
        Type m_type;
        union {
            internal::Vector *m_array;
            internal::Map    *m_dict;
            internal::String *m_strval;
            long long m_intval;
            double m_fval;
            bool  m_bool;
        };       

    public:
        void internal_build_string(const char *s, size_t l);
        void internal_build_object();
        void internal_build_array();
        void internal_build_bool(bool v);
        void internal_build_float(const char *s);
        void internal_build_integer(const char *s);
        Value *internal_add_key(const char *k, size_t l);
        Value *internal_add();
        Value *internal_parent;
    };

    namespace internal
    {
        struct MapData : public jmem_obj<MapData>
        {         
            struct pair
            {
                String key;
                Value value;
            };
            MapData();
            ~MapData();
            size_t size() const {return value_size;}            
            Value &operator[](const String &key);
            
            template <class VECT>
            void GetKeys(VECT *vec) const
            {
                for (size_t i = 0; i < value_size; i++)
                {
                    vec->push_back(buff[i].key.c_str());
                }
            }

            const Value &find(const String &key) const;

            int ref;
                        
        private:
            pair *buff;
            size_t value_size;  
            size_t buff_capacity;                        
        };

        class Map : public jmem_obj<Map>
        {
        public:
            Map():m_data(NULL){};
            Map(const Map &m)
            {
                data_copy(*this, m);
            }
            ~Map()
            {
                delete_data(m_data);
            }
            Map &operator=(const Map &s)
            {
                return data_set(*this, s);
            }
            size_t size() const
            {
                return data_size(m_data);
            } 
            Value &operator[](const String &k)
            {
                if (!m_data)
                {
                    m_data = new MapData;
                }
                return (*m_data)[k];
            }

            const Value &operator[](const String &k) const
            {
                return find(k);                
            }

            template <class VECT>
            void GetKeys(VECT *vec) const
            {
                if (m_data)
                {
                    m_data->GetKeys(vec);
                }
            }

            const Value &find(const String &k) const
            {
                if (m_data)
                {
                    return m_data->find(k);
                }
                return Value::Null;
            }
        private:         
            MapData *m_data;
            template <class T>
            friend inline void data_copy(T &l, const T &r);
            template <class T>
            friend T& data_set(T &l, const T &r);
        };      
    }
    
    inline Value *internal::Vector::push_back()
    {
        if (!m_data)
        {
            m_data = new VectorData;                    
        }
        m_data->increase_size();
        return &m_data->buff[m_data->size() - 1];
    }

    inline Value *internal::Vector::back()
    {
        size_t iSize = size();
        if (iSize > 0)
        {
            return &m_data->buff[iSize-1];
        }
        return NULL;
    }

    inline Value &internal::Vector::operator[](size_t idx)
    {
        return m_data->buff[idx];
    }

    inline internal::StringData::~StringData()
    {
        assert(ref == 0);
        jsfree(buff, buff_capacity);
    }

    inline Value::Value( const Value &v ) 
        :m_type(v.GetType()),internal_parent(NULL)
    {
        switch (m_type)
        {
        case JT_ARRAY:
            *m_array = *v.m_array;
            break;
        case JT_OBJECT:
            *m_dict = *v.m_dict;
            break;
        case JT_STRING:
            *m_strval = *v.m_strval;
            break;
        default:
            m_intval = v.m_intval;
        }
    }

    inline Value::~Value()
    {
        switch (m_type)
        {
        case JT_ARRAY:
            delete m_array;
            break;
        case JT_OBJECT:
            delete m_dict;
            break;
        case JT_STRING:
            delete m_strval;
            break;
        default:
            break;
        }
    }

    inline size_t Value::Size() const
    {
        if (m_type == JT_ARRAY)
        {
            assert(m_array);
            return m_array->size();
        }
        else if (m_type == JT_OBJECT)
        {
            assert(m_dict);
            return m_dict->size();
        }
        return 0;
    }

    template <class VECT>
    void Value::GetKeys(VECT *vec) const
    {
        if (m_type == JT_OBJECT)
        {
            assert(m_dict);
            m_dict->GetKeys(vec);
        }
    }

} // namespace tjson