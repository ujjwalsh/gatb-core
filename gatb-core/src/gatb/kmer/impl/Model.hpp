/*****************************************************************************
 *   GATB : Genome Assembly Tool Box
 *   Copyright (C) 2014  INRIA
 *   Authors: R.Chikhi, G.Rizk, E.Drezen
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

/** \file Model.hpp
 *  \date 01/03/2013
 *  \author edrezen
 *  \brief Kmer management
 */

#ifndef _GATB_CORE_KMER_IMPL_MODEL_HPP_
#define _GATB_CORE_KMER_IMPL_MODEL_HPP_

/********************************************************************************/

#include <gatb/system/api/Exception.hpp>
#include <gatb/kmer/api/IModel.hpp>

#include <gatb/tools/collections/api/Bag.hpp>

#include <gatb/tools/designpattern/api/Iterator.hpp>
#include <gatb/tools/designpattern/impl/IteratorHelpers.hpp>
#include <gatb/tools/misc/api/Data.hpp>
#include <gatb/tools/misc/api/Abundance.hpp>

#include <gatb/tools/math/LargeInt.hpp>

#include <vector>
#include <algorithm>
#include <iostream>

extern const char bin2NT[] ;
extern const char binrev[] ;
extern const unsigned char revcomp_4NT[];
extern const unsigned char comp_NT    [];

/********************************************************************************/
namespace gatb      {
namespace core      {
/** \brief Package for genomic databases management. */
namespace kmer      {
/** \brief Implementation for genomic databases management. */
namespace impl      {
/********************************************************************************/

#define KMER_DEFAULT_SPAN KSIZE_1

/********************************************************************************/

/** \brief Entry point for kmer management.
 *
 * This structure is only a container for other types defined inside. The specificity is
 * that this structure is templated by a 'span' integer that represents the maximal kmer
 * size supported (actually, the max value is 'span-1').
 *
 * Inside this structure, we have the following main elements:
 *      - 'Type'  : this is the integer type representing kmer values
 *      - 'Model' : provides many services for managing kmers
 *      - 'Count' : roughly speaking, this a kmer value with an associated abundance
 *
 * This structure must be used only with for 4 values (32,64,96,128 for instance, see Model.cpp), otherwise
 * a compilation error occurs (more values could be added in the future).
 *
 * A default value of 32 is defined for the template parameter, so writing 'Kmer<>::Model'
 * represents a model that supports kmers of size up to 31 (included).
 */
template <size_t span=KMER_DEFAULT_SPAN>
struct Kmer
{
    /************************************************************/
    /***********************     TYPE     ***********************/
    /************************************************************/

    /** Alias type for the integer value of a kmer. We use the LargeInt class for supporting big integers.
     * Note that the template parameter 'span' represents the maximal kmer size supported by the Kmer class.
     * A conversion to the template parameter of LargeInt is done.
     */
    typedef tools::math::LargeInt<(span+31)/32> Type;


    /************************************************************/
    /***********************     MODEL    ***********************/
    /************************************************************/

    /** Forward declarations. */
    class ModelDirect;
    class ModelCanonical;
    template<class Model, class Comparator> class ModelMinimizer;

    /** Now, we need to define what is a kmer for each kind of model.
     *
     * The simple case is KmerDirect, where only the value of the kmer is available
     * as a method 'value' returning a Type object.
     *
     * The second case is KmerCanonical, which is the same as KmerDirect, but with
     * two other methods 'forward' and 'revcomp'
     *
     * The third case is KmerMinimizer<Model> which allows to handle minimizers associated
     * to a kmer. This class inherits from the Model::Kmer type and adds methods specific
     * to minimizers, such as 'minimizer' itself (ie the Model::Kmer object holding the
     * minimizer), 'position' giving the position of the minimizer whithin the kmer and
     * 'hasChanged' telling whether a minimizer has changed during iteration of kmers from
     * some data source (a sequence data for instance).
     */

    /** Kmer type for the ModelDirect class. */
    class KmerDirect
    {
    public:
        /** Returns the value of the kmer.
         * \return the kmer value as a Type object. */
        const Type& value  () const { return _value;   }

        /** Comparison operator between two instances.
         * \param[in] t : object to be compared to
         * \return true if the values are the same, false otherwise. */
        bool operator< (const KmerDirect& t) const  { return this->_value < t._value; };

        /** Set the value of the kmer
         * \param[in] val : value to be set. */
        void set (const Type& val) { _value=val; }

        /** Tells whether the kmer is valid or not. It may be invalid if some unwanted
         * nucleotides characters (like N) have been used to build it.
         * \return true if valid, false otherwise. */
        bool isValid () const { return _isValid; }

    protected:
        Type _value;
        bool _isValid;
        friend class ModelDirect;

        /** Extract a mmer from a kmer. This is done by using a mask on the kmer.
         * \param[in] mask : mask to be applied to the current kmer
         * \param[in] size : shift size (needed for some kmer classes but not all)
         * \return the extracted kmer.
         */
        KmerDirect extract      (const Type& mask, size_t size, Type * mmer_lut)  {  KmerDirect output;  output.set (this->value() & mask);  return output;  }
        KmerDirect extractShift (const Type& mask, size_t size, Type * mmer_lut)  {  KmerDirect output = extract(mask,size,mmer_lut);  _value = _value >> 2;  return output;  }
    };

    /** Kmer type for the ModelCanonical class. */
    class KmerCanonical
    {
    public:

        /** Returns the value of the kmer.
         * \return the kmer value as a Type object. */
        const Type& value  () const { return table[choice];   }

        /** Comparison operator between two instances.
         * \param[in] t : object to be compared to
         * \return true if the values are the same, false otherwise. */
        bool operator< (const KmerDirect& t) const  { return this->value() < t.value(); };

        /** Set the value of the kmer
         * \param[in] val : value to be set. */

        void set (const Type& val)
        {
            /** Not really a forward/revcomp couple, but may be useful for the minimizer default value. */
            set (val,val);
        }

        /** Set the forward/revcomp attributes. */
        void set (const Type& forward, const Type& revcomp)
        {
            table[0]=forward;
            table[1]=revcomp;
            updateChoice ();
        }

		
        /** Tells whether the kmer is valid or not. It may be invalid if some unwanted
         * nucleotides characters (like N) have been used to build it.
         * \return true if valid, false otherwise. */
        bool isValid () const { return _isValid; }

        /** Returns the forward value of this canonical kmer.
         * \return the forward value */
        const Type& forward() const { return table[0]; }

        /** Returns the reverse complement value of this canonical kmer.
         * \return the reverse complement value */
        const Type& revcomp() const { return table[1]; }

        /** Tells which strand is used for the kmer.
         * \return true if the kmer value is the forward value, false if it is the reverse complement value
         */
        bool which () const { return choice==0 ? true : false; }

    protected:
        Type table[2];  char choice;
		
        bool _isValid;
        void updateChoice () { choice = (table[0] < table[1]) ? 0 : 1; }
        friend class ModelCanonical;

        /** Extract a mmer from a kmer. This is done by using a mask on the kmer.
         * \param[in] mask : mask to be applied to the current kmer
         * \param[in] size : shift size (needed for some kmer classes but not all)
         * \return the extracted kmer.
         */
        KmerCanonical extract (const Type& mask, size_t size, Type * mmer_lut)
        {

            KmerCanonical output;
			
			output.set(mmer_lut[(this->table[0] & mask).getVal()]); //no need to recomp updateChoice with this
			//mmer_lut takes care of revcomp and forbidden mmers
			//output.set (this->table[0] & mask, (this->table[1] >> size) & mask);
            //output.updateChoice();
            return output;
        }


        KmerCanonical extractShift (const Type& mask, size_t size, Type * mmer_lut)
        {
            KmerCanonical output = extract (mask, size,mmer_lut);
            table[0] = table[0] >> 2;   table[1] = table[1] << 2;  updateChoice();
            return output;
        }
    };

    /** Kmer type for the ModelMinimizer class. */
    template<class Model, class Comparator>
    class KmerMinimizer : public Model::Kmer
    {
    public:

        /** Returns the minimizer of the current kmer as a Model::Kmer object
         * \return the Model::Kmer instance */
        const typename Model::Kmer& minimizer() const  {  return _minimizer; }

        /** Returns the position of the minimizer within the kmer. By convention,
         * a negative value means that there is no minimizer inside the kmer.
         * \return the position of the minimizer. */
        int position () const  {  return _position;  }

        /** Tells whether the minimizer has changed; useful while iterating kmers
         * \return true if changed, false otherwise */
        bool hasChanged () const  {  return _changed;  }

    protected:

        typename Model::Kmer _minimizer;
        int16_t              _position;
        bool                 _changed;
        friend class ModelMinimizer<Model,Comparator>;
    };

    /** Shortcut.
     *  - first  : the nucleotide value (A=0, C=1, T=2, G=3)
     *  - second : 0 if valid, 1 if invalid (in case of N character for instance) */
    typedef std::pair<char,char> ConvertChar;


    /** Abstract class that provides kmer management.
     *
     * This class is the base class for kmer management. It provides several services on this purpose
     * like getting kmer information from some nucleotides sequence, or iterate kmers through such
     * a sequence.
     *
     * This class has two templates types :
     *
     *      1) ModelImpl : ModelAbstract is design for static polymorphism and ModelImpl is the implementation
     *                     that must be provided to it
     *
     *      2) T : type of kmers handled by the class (ie KmerDirect, KmerCanonical...); I was not successful
     *             in trying to hide KmerXXX classes in the dedicated ModelXXX classes because of mutual
     *             dependencies while template specializations (maybe a solution one day)
     *
     * End user will be given instances of Kmer class, delivering more or less information according to the
     * specific type of ModelImpl
     */
    template <class ModelImpl, typename T>
    class ModelAbstract : public system::SmartPointer
    {
    public:

        /** Type of kmers provided by the class. */
        typedef T Kmer;

        /** (default) Constructor. The provided (runtime) kmer size must be coherent with the span (static) value.
         * \param[in] sizeKmer : size of kmers handled by the instance.*/
        ModelAbstract (size_t sizeKmer=span-1) : _kmerSize(sizeKmer)
        {
            /** We check that the Type precision is enough for the required kmers span. */
            if (sizeKmer >= span)
            {
                throw system::Exception ("Type '%s' has too low precision (%d bits) for the required %d kmer size",
                    Type().getName(), Type().getSize(), sizeKmer
                );
            }

            /** We compute the mask of the kmer. Useful for computing kmers in a recursive way. */
            Type un = 1;
            _kmerMask = (un << (_kmerSize*2)) - un;

            size_t shift = 2*(_kmerSize-1);

            /** The _revcompTable is a shortcut used while computing revcomp recursively. */
            /** Important: don't forget the Type cast, otherwise the result in only on 32 bits. */
            for (size_t i=0; i<4; i++)   {  Type tmp  = comp_NT[i];  _revcompTable[i] = tmp << shift;  }
        }

        /** Returns the span of the model
         * \return the model span. */
        size_t getSpan () const { return span; }

        /** Get the memory size (in bytes) of a Kmer<span>::Type object.
         * \return the memory size of a kmer. */
        size_t getMemorySize ()  const  { return sizeof (Type); }

        /** Gives the kmer size for this model.
         * \return the kmer size. */
        size_t getKmerSize () const { return _kmerSize; }

        /** Gives the maximum value of a kmer for the instance.
         * \return the maximum kmer value. */
        const Type& getKmerMax () const { return _kmerMask; }

        /** Returns an ascii representation of the kmer value.
         * \param[in] kmer : the kmer we want an ascii representation for
         * \return a string instance holding the ascii representation. */
        std::string toString (const Type& kmer) const  {  return kmer.toString(_kmerSize);  }

        /** Compute the reverse complement of a kmer.
         * \param[in] kmer : the kmer to be reverse-completed.
         * \return the reverse complement. */
        Type reverse (const Type& kmer)  const  { return revcomp (kmer, this->_kmerSize); }

        /** Build a kmer from a Data object (ie a sequence of nucleotides), starting at an index in the nucleotides sequence.
         * The result is a pair holding the built kmer and a boolean set to yes if the built kmer has to be understood in
         * the forward sense, false otherwise.
         * \param[in] data : the data from which we extract a kmer
         * \param[in] idx : start index in the data object (default to 0)
         * \return a pair with the built kmer and a boolean set to yes if the kmer is understood in the forward strand
         */
        Kmer getKmer (const tools::misc::Data& data, size_t idx=0)  const
        {
            return codeSeed (data.getBuffer() + idx, data.getEncoding());  // should not work with BINARY encoding
        }

        /** Iteration of the kmers from a data object through a functor (so lambda expressions can be used).
         * \param[in] data : the sequence of nucleotides.
         * \param[in] fct  : functor that handles one kmer */
        template<typename Callback>
        bool iterate (tools::misc::Data& data, Callback callback) const
        {
            return execute <Functor_iterate<Callback> > (data.getEncoding(), Functor_iterate<Callback>(data,callback));
        }

        /** Compute the kmer given some nucleotide data.
         *  Note that we don't check if we have enough nucleotides in the provided data.
         * \param[in] seq : the sequence
         * \param[in] encoding : encoding mode of the sequence
         * \return the kmer for the given nucleotides. */
        Kmer codeSeed (const char* seq, tools::misc::Data::Encoding_e encoding) const
        {
            return execute<Functor_codeSeed> (encoding, Functor_codeSeed(seq));
        }

        /** Compute the next right kmer given a current kmer and a nucleotide.
         * \param[in] kmer : the current kmer as a starting point
         * \param[in] nucl : the next nucleotide
         * \param[in] encoding : encoding mode of the sequence
         * \return the kmer on the right of the given kmer. */
        Kmer codeSeedRight (const Kmer& kmer, char nucl, tools::misc::Data::Encoding_e encoding)  const
        {
            return execute<Functor_codeSeedRight> (encoding, Functor_codeSeedRight(kmer,nucl));
        }

        /** Build a vector of successive kmers from a given sequence of nucleotides provided as a Data object.
         * \param[in] data : the sequence of nucleotides.
         * \param[out] kmersBuffer : the successive kmers built from the data object.
         * \return true if kmers have been extracted, false otherwise. */
		//GR : est ce quon pourrait passer un pointeur (de taille suffisante) au lieu  vector, pour pas avoir a faire resize dessus dans tas
		// si taille pas suffisante,
        bool build (tools::misc::Data& data, std::vector<Kmer>& kmersBuffer)  const
        {
            /** We compute the number of kmers for the provided data. Note that we have to check that we have
             * enough nucleotides according to the current kmer size. */
            int32_t nbKmers = data.size() - this->getKmerSize() + 1;
            if (nbKmers <= 0)  { return false; }

            /** We resize the resulting kmers vector. */
            kmersBuffer.resize (nbKmers);

            /** We fill the vector through a functor. */
            this->iterate (data, BuildFunctor<Kmer>(kmersBuffer));

            return true;
        }

        /** Iterate the neighbors of a given kmer; these neighbors are:
         *  - 4 outgoing neighbors
         *  - 4 incoming neighbors.
         *  This method uses a functor that will be called for each possible neighbor of the source kmer.
         *  \param[in] source : the kmer from which we want neighbors.
         *  \param[in] fct : a functor called for each neighbor.
         *  \param[in] mask : holds 8 bits for each possible neighbor (1 means that the neighbor is computed)
         */
        template<typename Functor>
        void iterateNeighbors (const Type& source, const Functor& fct, u_int8_t mask=0xFF)  const
        {
            // hacky to cast Functor& instead of const Functor&, but don't wanna break API yet want non-const functor
            iterateOutgoingNeighbors(source, (Functor&) fct, (mask>>0) & 15);
            iterateIncomingNeighbors(source, (Functor&) fct, (mask>>4) & 15);
        }

        /** Iterate the neighbors of a given kmer; these neighbors are:
         *  - 4 outcoming neighbors
         *  This method uses a functor that will be called for each possible neighbor of the source kmer.
         *  \param[in] source : the kmer from which we want neighbors.
         *  \param[in] fct : a functor called for each neighbor.*/
        template<typename Functor>
        void iterateOutgoingNeighbors (const Type& source, Functor& fct, u_int8_t mask=0x0F)  const
        {
            /** We compute the 4 possible neighbors. */
            for (size_t nt=0; nt<4; nt++)
            {
                if (mask & (1<<nt))
                {
                    Type next1 = (((source) * 4 )  + nt) & getKmerMax();
                    Type next2 = revcomp (next1, getKmerSize());
                    fct (std::min (next1, next2));
                }
            }
        }

        /** Iterate the neighbors of a given kmer; these neighbors are:
         *  - 4 incoming neighbors
         *  This method uses a functor that will be called for each possible neighbor of the source kmer.
         *  \param[in] source : the kmer from which we want neighbors.
         *  \param[in] fct : a functor called for each neighbor.*/
        template<typename Functor>
        void iterateIncomingNeighbors (const Type& source, Functor& fct, u_int8_t mask=0x0F)  const
        {
            Type rev = core::tools::math::revcomp (source, getKmerSize());

            /** We compute the 4 possible neighbors. */
            for (size_t nt=0; nt<4; nt++)
            {
                /** Here, we use the complement of the current nucleotide 'nt'.
                 * Remember : A=0, C=1, T=2, G=3  (each coded on 2 bits)
                 * => we can get the complement by negating the most significant bit (ie "nt^2") */

                if (mask & (1<<nt))
                {
                    Type next1 = (((rev) * 4 )  + (nt^2)) & getKmerMax();
                    Type next2 = revcomp (next1, getKmerSize());
                    fct (std::min (next1, next2));
                }
            }
        }

        /************************************************************/
        /** \brief Iterator on successive kmers
         *
         * This class will iterate successive kmers extracted from a Data object.
         * It is similar to the Model::build, except that here we don't have a container
         * holding all the successive kmers (ie. we have here only sequential access and
         * not direct access).
         *
         * To be used, such an iterator must be initialized with some sequence of nucleotides,
         * which is done with the 'setData' method.
         */
        class Iterator : public tools::dp::impl::VectorIterator<Kmer>
        {
        public:
            /** Constructor.
             * \param[in] ref : the associated model instance.
             */
            Iterator (ModelAbstract& ref)  : _ref(ref)   {}

            /** Set the data to be iterated.
             * \param[in] d : the data as information source for the iterator
             */
            void setData (tools::misc::Data& d)
            {
                /** We fill the vector with the items to be iterated. */
                _ref.build (d, this->_items);

                /** We set the vector size. */
                this->_nb = this->_items.size();
            }

        private:
            /** Reference on the underlying model; called for its 'build' method. */
            ModelAbstract& _ref;
        };

    protected:

        /** Size of a kmer for this model. */
        size_t  _kmerSize;

        /** Mask for the kmer. Used for computing recursively kmers. */
        Type  _kmerMask;

        /** Shortcut for easing/speeding up the recursive revcomp computation. */
        Type _revcompTable[4];

        /** Note for the ASCII conversion: the 4th bit is used to tell whether it is invalid or not.
         * => it finds out that 'N' character has this 4th bit equals to 1, which is not the case
         * for 'A', 'C', 'G' and 'T'. */
        struct ConvertASCII    { static ConvertChar get (const char* buffer, size_t idx)  { return ConvertChar((buffer[idx]>>1) & 3, (buffer[idx]>>3) & 1); }};
        struct ConvertInteger  { static ConvertChar get (const char* buffer, size_t idx)  { return ConvertChar(buffer[idx],0); }         };
        struct ConvertBinary   { static ConvertChar get (const char* buffer, size_t idx)  { return ConvertChar(((buffer[idx>>2] >> ((3-(idx&3))*2)) & 3),0); } };

        /** \return -1 if valid, otherwise index of the last found bad character. */
        template<class Convert>
        int polynom (const char* seq, Type& kmer)  const
        {
            ConvertChar c;
            int badIndex = -1;

            /** We iterate 'kmersize" nucleotide to build the first kmer as a polynomial evaluation. */
            kmer = 0;
            for (int i=0; i<_kmerSize; ++i)
            {
                /** We get the current nucleotide (and its invalid status). */
                c = Convert::get(seq,i);

                /** We update the polynome value. */
                kmer = (kmer<<2) + c.first;

                /** We update the 'invalid' status: a single bad character makes the result invalid. */
                if (c.second)  { badIndex = i; }
            }

            return badIndex;
        }

        /** Generic function that switches to the correct implementation according to the encoding scheme
         * of the provided Data parameter; the provided functor class is specialized with the correct data conversion type
         * and the called.
         */
        template<class Functor>
        typename Functor::Result execute (tools::misc::Data::Encoding_e encoding, Functor action) const
        {
            switch (encoding)
            {
                case  tools::misc::Data::ASCII:    return action.template operator()<ConvertASCII  > (this);
                case  tools::misc::Data::INTEGER:  return action.template operator()<ConvertInteger> (this);
                case  tools::misc::Data::BINARY:   return action.template operator()<ConvertBinary>  (this);
                default:  throw system::Exception ("BAD FORMAT IN 'execute'");
            }
        }

        /** Adaptor between the 'execute' method and the 'codeSeed' method. */
        struct Functor_codeSeed
        {
            typedef typename ModelImpl::Kmer Result;
            const char* buffer;
            Functor_codeSeed (const char* buffer) : buffer(buffer) {}
            template<class Convert>  Result operator() (const ModelAbstract* model)
            {
                Result result;
                static_cast<const ModelImpl*>(model)->template first <Convert> (buffer, result);
                return result;
            }
        };

        /** Adaptor between the 'execute' method and the 'codeSeedRight' method. */
        struct Functor_codeSeedRight
        {
            typedef typename ModelImpl::Kmer Result;
            const Kmer& kmer; char nucl;
            Functor_codeSeedRight (const Kmer& kmer, char nucl) : kmer(kmer), nucl(nucl) {}
            template<class Convert>  Result operator() (const ModelAbstract* model)
            {
                ConvertChar c = Convert::get(&nucl,0);
                Result result=kmer;
                static_cast<const ModelImpl*>(model)->template next <Convert> (c.first, result, c.second==0);
                return result;
            }
        };

        /** Adaptor between the 'execute' method and the 'iterate' method. */
        template<class Callback>
        struct Functor_iterate
        {
            typedef bool Result;
            tools::misc::Data& data; Callback callback;
            Functor_iterate (tools::misc::Data& data, Callback callback) : data(data), callback(callback) {}
            template<class Convert>  Result operator() (const ModelAbstract* model)
            {
                return static_cast<const ModelImpl*>(model)->template iterate<Callback, Convert> (data.getBuffer(), data.size(), callback);
            }
        };

        /** Template method that iterates the kmer of a given Data instance.
         *  Note : we use static polymorphism here (http://en.wikipedia.org/wiki/Template_metaprogramming)
         */
        template<typename Callback, typename Convert>
        bool iterate (const char* seq, size_t length, Callback callback) const
        {
            /** We compute the number of kmers for the provided data. Note that we have to check that we have
             * enough nucleotides according to the current kmer size. */
            int32_t nbKmers = length - _kmerSize + 1;
            if (nbKmers <= 0)  { return false; }

            /** We create a result instance. */
            typename ModelImpl::Kmer result;

            /** We compute the initial seed from the provided buffer. */
            int indexBadChar = static_cast<const ModelImpl*>(this)->template first<Convert> (seq, result);

            /** We need to keep track of the computed kmers. */
            size_t idxComputed = 0;

            /** We notify the result. */
            this->notification<Callback> (result, idxComputed, callback);

            /** We compute the following kmers from the first one.
             * We have consumed 'kmerSize' nucleotides so far for computing the first kmer,
             * so we start the loop with idx=_kmerSize.
             */
            for (size_t idx=_kmerSize; idx<length; idx++)
            {
                /** We get the current nucleotide. */
                ConvertChar c = Convert::get (seq, idx);

                if (c.second)  { indexBadChar = _kmerSize-1; }
                else           { indexBadChar--;     }

                /** We compute the next kmer from the previous one. */
                static_cast<const ModelImpl*>(this)->template next<Convert> (c.first, result, indexBadChar<0);

                /** We notify the result. */
                this->notification<Callback> (result, ++idxComputed, callback);
            }

            return true;
        }

        template <class Callcack>
        void  notification (const Kmer& value, size_t idx, Callcack callback) const {  callback (value, idx);  }

        /** */
        template<typename Type>
        struct BuildFunctor
        {
            std::vector<Type>& kmersBuffer;
            BuildFunctor (std::vector<Type>& kmersBuffer) : kmersBuffer(kmersBuffer) {}
            void operator() (const Type& kmer, size_t idx)  {  kmersBuffer[idx] = kmer;  }
        };
		
    };

    /********************************************************************************/

    /** \brief Model that handles "direct" kmers, ie sequences of nucleotides.
     * The associated value of such a kmer is computed as a polynom P(X) with X=4
     * and where the coefficients are in [0..3].
     * By convention, we use A=0, C=1, T=2 and G=3
     */
    class ModelDirect :  public ModelAbstract<ModelDirect, Kmer<span>::KmerDirect>
    {
    public:

        /** Type holding all the information of a kmer.  */
        typedef Kmer<span>::KmerDirect Kmer;

        /** Constructor.
         * \param[in] kmerSize : size of the kmers handled by the model. */
        ModelDirect (size_t kmerSize=span-1) : ModelAbstract<ModelDirect, Kmer> (kmerSize) {}

        /** Computes a kmer from a buffer holding nucleotides encoded in some format.
         * The way to interpret the buffer is done through the provided Convert template class.
         * \param[in] buffer : holds the nucleotides sequence from which the kmer has to be computed
         * \param[out] value : kmer as a result
         */
        template <class Convert>
        int first (const char* buffer, Kmer& value)   const
        {
           int result = this->template polynom<Convert> (buffer, value._value);
            value._isValid = result < 0;
            return result;
        }

        /** Computes a kmer in a recursive way, ie. from a kmer and the next
         * nucleotide. The next nucleotide is computed from a buffer and the
         * index of the nucleotide within the buffer.
         * The way to interpret the buffer is done through the provided Convert template class.
         * \param[in] buffer : holds the nucleotides sequence from which the kmer has to be computed
         * \param[in] idx : index of the nucleotide within the buffer
         * \param[out] value kmer as a result
         */
        template <class Convert>
        void  next (char c, Kmer& value, bool isValid)   const
        {
            value._value   = ( (value._value << 2) +  c) & this->_kmerMask;
            value._isValid = isValid;
        }
	};

    /********************************************************************************/

    /** \brief Model that handles "canonical" kmers, ie the minimum value of the
     * direct kmer and its reverse complement.
     */
    class ModelCanonical :  public ModelAbstract<ModelCanonical, Kmer<span>::KmerCanonical>
    {
    public:

        /** Type holding all the information of a kmer.  */
        typedef Kmer<span>::KmerCanonical Kmer;

        /** Constructor.
         * \param[in] kmerSize : size of the kmers handled by the model. */
        ModelCanonical (size_t kmerSize=span-1) : ModelAbstract<ModelCanonical, Kmer> (kmerSize) {}

        /** Computes a kmer from a buffer holding nucleotides encoded in some format.
         * The way to interpret the buffer is done through the provided Convert template class.
         * \param[in] buffer : holds the nucleotides sequence from which the kmer has to be computed
         * \param[out] value : kmer as a result
         */
        template <class Convert>
        int first (const char* seq, Kmer& value)   const
        {

            int result = this->template polynom<Convert> (seq, value.table[0]);
            value._isValid = result < 0;
            value.table[1] = this->reverse (value.table[0]);
            value.updateChoice();
            return result;
        }

        /** Computes a kmer in a recursive way, ie. from a kmer and the next
         * nucleotide. The next nucleotide is computed from a buffer and the
         * index of the nucleotide within the buffer.
         * The way to interpret the buffer is done through the provided Convert template class.
         * \param[in] buffer : holds the nucleotides sequence from which the kmer has to be computed
         * \param[in] idx : index of the nucleotide within the buffer
         * \param[out] value kmer as a result
         */
        template <class Convert>
        void  next (char c, Kmer& value, bool isValid)   const
        {
            value.table[0] = ( (value.table[0] << 2) +  c                     ) & this->_kmerMask;
            value.table[1] = ( (value.table[1] >> 2) +  this->_revcompTable[c]) & this->_kmerMask;
            value._isValid = isValid;

            value.updateChoice();
        }
    };

    /********************************************************************************/

    struct ComparatorMinimizer
    {
        template<class Model>  void init (const Model& model, Type& best) const { best = model.getKmerMax(); }
        bool operator() (const Type& current, const Type& best) const { return current < best; }
    };

    /** \brief Model that handles kmers of the Model type + a minimizer
     *
     * This model supports the concept of minimizer. It acts as a Model instance (given as a
     * template class) and add minimizer information to the Kmer type.
     */
    template<class ModelType, class Comparator=Kmer<span>::ComparatorMinimizer>
    class ModelMinimizer :  public ModelAbstract <ModelMinimizer<ModelType,Comparator>, KmerMinimizer<ModelType,Comparator> >
    {
    public:

        /** Type of the model for kmer and mmers.  */
        typedef ModelType Model;

        /** Type holding all the information of a kmer.  */
        typedef KmerMinimizer<ModelType,Comparator> Kmer;

        /** Return a reference on the model used for managing mmers. */
        const ModelType& getMmersModel() const { return _miniModel; }

        /** Constructor.
         * \param[in] kmerSize      : size of the kmers handled by the model.
         * \param[in] minimizerSize : size of the mmers handled by the model. */
        ModelMinimizer (size_t kmerSize, size_t minimizerSize, Comparator cmp=Comparator())
            : ModelAbstract <ModelMinimizer<ModelType,Comparator>, Kmer > (kmerSize),
              _kmerModel(kmerSize), _miniModel(minimizerSize), _cmp(cmp)
        {
            if (kmerSize <= minimizerSize)  { throw system::Exception ("Bad values for kmer %d and minimizer %d", kmerSize, minimizerSize); }

            /** We compute the number of mmers found in a kmer. */
            _nbMinimizers = _kmerModel.getKmerSize() - _miniModel.getKmerSize() + 1;

            /** We need a mask to extract a mmer from a kmer. */

            _mask  = ((u_int64_t)1 << (2*_miniModel.getKmerSize())) - 1;
            _shift = 2*(_nbMinimizers-1);

            /** We initialize the default value of the minimizer.
             * The value is actually set by the Comparator instance provided as a template of the class. */
            Type tmp;
            _cmp.template init<ModelType> (getMmersModel(), tmp);
            _minimizerDefault.set (tmp);
			
			u_int64_t nbminims_total = ((u_int64_t)1 << (2*_miniModel.getKmerSize()));
			_mmer_lut = (Type *) malloc(sizeof(Type) * nbminims_total ); //free that in destructor

			for(int ii=0; ii< nbminims_total; ii++)
			{
				Type mmer = ii;
				Type rev_mmer = revcomp(mmer, minimizerSize);
				
                // if(!is_allowed(mmer.getVal(),minimizerSize)) mmer = _mask;
                // if(!is_allowed(rev_mmer.getVal(),minimizerSize)) rev_mmer = _mask;
				
				if(rev_mmer < mmer) mmer = rev_mmer;
				
				if (!is_allowed(mmer.getVal(),minimizerSize)) mmer = _mask;

				_mmer_lut[ii] = mmer;
			}
        }

        /** */
        ~ModelMinimizer ()
        {
            if (_mmer_lut != 0)  { free (_mmer_lut); }
        }

        template <class Convert>

        int first (const char* seq, Kmer& kmer)   const
        {
            /** We compute the first kmer. */
            int result = _kmerModel.template first<Convert> (seq, kmer);

            /** We compute the minimizer of the kmer. */
            computeNewMinimizer (kmer);

            return result;
        }

        template <class Convert>
        void  next (char c, Kmer& kmer, bool isValid)   const
        {
            /** We compute the next kmer. */
            _kmerModel.template next<Convert> (c, kmer, isValid);

            /** We set the valid status according to the Convert result. */
            kmer._isValid = isValid;

            /** We extract the new mmer from the kmer. */
            typename ModelType::Kmer mmer = kmer.extract (this->_mask, this->_shift,_mmer_lut);

            /** We update the position of the previous minimizer. */
            kmer._position--;

            /** By default, we consider that the minimizer is still the same. */
            kmer._changed  = false;

            /** We have to update the minimizer in the following case:
             *      1) the new mmer is the new minimizer
             *      2) the previous minimizer is invalid or out from the new kmer window.
             */
            if (_cmp (mmer.value(), kmer._minimizer.value()) == true) // .value()
            {
                kmer._minimizer = mmer; //ici intercalet une lut pour revcomp et minim interdits
                kmer._position  = _nbMinimizers - 1;
                kmer._changed   = true;
            }

            else if (kmer._position < 0)
            {
                computeNewMinimizer (kmer);
            }
        }

        /** */
        u_int64_t getMinimizerValue (const Type& k) const
        {
            Kmer km; km.set(k);  this->computeNewMinimizer (km);
            return km.minimizer().value().getVal();
        }

    private:
        ModelType  _kmerModel;
        ModelType  _miniModel;
        Comparator _cmp;
        size_t     _nbMinimizers;
        Type       _mask;

		Type * _mmer_lut;
        size_t     _shift;
        typename ModelType::Kmer _minimizerDefault;

        /** Tells whether a minimizer is valid or not, in order to skip minimizers
         *  that are too frequent. */
        bool is_allowed (uint32_t mmer, uint32_t len)
        {
            u_int64_t  _mmask_m1  ;
            u_int64_t  _mask_0101 ;
            u_int64_t  _mask_ma1 ;

            _mmask_m1  = (1 << ((len-2)*2)) -1 ;
            _mask_0101 = 0x5555555555555555  ;
            _mask_ma1  = _mask_0101 & _mmask_m1;

            u_int64_t a1 = mmer;
            a1 =   ~(( a1 )   | (  a1 >>2 ));
            a1 =((a1 >>1) & a1) & _mask_ma1 ;

            if(a1 != 0) return false;

            // if ((mmer & 0x3f) == 0x2a)   return false;   // TTT suffix
            // if ((mmer & 0x3f) == 0x2e)   return false;   // TGT suffix
            // if ((mmer & 0x3c) == 0x28)   return false;   // TT* suffix
            // for (uint32_t j = 0; j < len - 3; ++j)       // AA inside
            //      if ((mmer & 0xf) == 0)  return false;
            //      else                    mmer >>= 2;
            // if (mmer == 0)               return false;   // AAA prefix
            // if (mmer == 0x04)            return false;   // ACA prefix
            // if ((mmer & 0xf) == 0)   return false;       // *AA prefix

            return true;
        }
        
                /** Returns the minimizer of the provided vector of mmers. */
        void computeNewMinimizer (Kmer& kmer) const
        {
            /** We update the attributes of the provided kmer. Note that an invalid minimizer is
             * memorized by convention by a negative minimizer position. */
            kmer._minimizer = this->_minimizerDefault;
            kmer._position  = -1;
            kmer._changed   = true;

            /** We need a local object that loops each mmer of the provided kmer (and we don't want
             * to modify the kmer value of this provided kmer). */
            Kmer loop = kmer;

            typename ModelType::Kmer mmer;

            /** We compute each mmer and memorize the minimizer among them. */

            for (int16_t idx=_nbMinimizers-1; idx>=0; idx--)
            {
                /** We extract the most left mmer in the kmer. */
                mmer = loop.extractShift (_mask, _shift, _mmer_lut);

                /** We check whether this mmer is the new minimizer. */
                if (_cmp (mmer.value(), kmer._minimizer.value()) == true)  {  kmer._minimizer = mmer;   kmer._position = idx;  }
            }
        }
    };

    /************************************************************/
    /*********************  SUPER KMER    ***********************/
    /************************************************************/
    class SuperKmer
    {
    public:

        //typedef Type SType[2];

        typedef ModelMinimizer<ModelCanonical> Model;
        typedef typename Model::Kmer           Kmer;

        static const u_int64_t DEFAULT_MINIMIZER = 1000000000 ;

        SuperKmer (size_t kmerSize, size_t miniSize, std::vector<Kmer>&  kmers)
            : kmerSize(kmerSize), miniSize(miniSize), minimizer(DEFAULT_MINIMIZER), kmers(kmers), range(0,0)
        {
            if (kmers.empty())  { kmers.resize(kmerSize); range.second = kmers.size()-1; }
        }

        u_int64_t                minimizer;
        std::pair<size_t,size_t> range;

        Kmer& operator[] (size_t idx)  {  return kmers[idx+range.first];  }

        size_t size() const { return range.second - range.first + 1; }

        bool isValid() const { return minimizer != DEFAULT_MINIMIZER; }

        /** */
        void save (tools::collections::Bag<Type>& bag)
        {
            size_t superKmerLen = size();

            int64_t zero = 0;
            Type masknt ((int64_t) 3);
            Type radix, radix_kxmer_forward ,radix_kxmer ;
            Type nbK ((int64_t) size());
            Type compactedK(zero);

            for (size_t ii=1 ; ii < superKmerLen; ii++)
            {
                compactedK = compactedK << 2  ;
                compactedK = compactedK | ( ((*this)[ii].forward()) & masknt) ;
            }

            int maxs = (compactedK.getSize() - 8 ) ;

            compactedK = compactedK | (  nbK << maxs ) ;

            bag.insert (compactedK);
            bag.insert ((*this)[0].forward());
        }

        /** NOT USED YET. */
        void load (tools::dp::Iterator<Type>& iter)
        {
            Type superk = iter.item(); iter.next();
            Type seedk = iter.item();

            u_int8_t        nbK, rem ;
            Type compactedK;
            int ks = kmerSize;
            Type un = 1;
            size_t _shift_val = Type::getSize() -8;
            Type kmerMask = (un << (ks*2)) - un;
            size_t shift = 2*(ks-1);

            compactedK =  superk;
            nbK = (compactedK >> _shift_val).getVal() & 255; // 8 bits poids fort = cpt //todo for large k values
            rem = nbK;

            Type temp = seedk;
            Type rev_temp = revcomp(temp,ks);
            Type newnt ;
            Type mink;

            /** We loop over each kmer of the current superkmer. */
            for (int ii=0; ii<nbK; ii++,rem--)
            {
                mink = std::min (rev_temp, temp);

                /** We set the current (canonical) kmer. */
                kmers[ii].set (rev_temp, temp);

                if(rem < 2) break;
                newnt =  ( superk >> ( 2*(rem-2)) ) & 3 ;

                temp = ((temp << 2 ) |  newnt   ) & kmerMask;
                newnt =  Type(comp_NT[newnt.getVal()]) ;
                rev_temp = ((rev_temp >> 2 ) |  (newnt << shift) ) & kmerMask;
            }
        }

    private:
        size_t              kmerSize;
        size_t              miniSize;
        std::vector<Kmer>&  kmers;
    };

    /************************************************************/
    /***********************     COUNT    ***********************/
    /************************************************************/
    /** \brief Structure associating a kmer value with an abundance value.
     *
     * This structure is useful for methods that counts kmer, such as the SortingCount algorithm.
     * It is also interesting to save [kmer,abundance] in a HDF5 format.
     *
     * By default, the abundance value is coded on 16 bits, so abundance up to 1<<16 can be used.
     */
    struct Count : tools::misc::Abundance<Type,u_int16_t>
    {
        /** Shortcut. */
        typedef u_int16_t Number;

        /** Constructor.
         * \param[in] val : integer value of the kmer
         * \param[in] abund : abundance for the kmer */
        Count(const Type& val, const Number& abund) : tools::misc::Abundance<Type,Number>(val, abund) {}

        /** Default constructor. */
        Count() : tools::misc::Abundance<Type,Number>(Type(), 0) {}

        /** Copy constructor. */
        Count(const Count& val) : tools::misc::Abundance<Type,Number>(val.value, val.abundance) {}

        /** Comparison operator
         * \param[in] other : object to be compared to
         * \return true if the provided kmer value is greater than the current one. */
        bool operator< (const Count& other) const {  return this->value < other.value; }
        
        /** Equal operator
         * \param[in] other : object to be compared to
         * \return true if the provided kmer value is greater than the current one. */
        bool operator== (const Count& other) const {  return (this->value == other.value && this->abundance == other.abundance); }
    };

};  // struct Kmer

/********************************************************************************/
} } } } /* end of namespaces. */
/********************************************************************************/

#endif /* _GATB_CORE_KMER_IMPL_MODEL_HPP_ */
