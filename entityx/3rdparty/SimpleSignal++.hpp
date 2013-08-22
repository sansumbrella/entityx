#ifndef SIMPLE_SIGNALPP_H__
#define SIMPLE_SIGNALPP_H__


#include <vector>
#include <algorithm>
#include <utility>

namespace simpsig {

    template<typename... Args> class Signal;
    template<typename... Args> class Slot;

    template<typename... Args>
    class Signal {
    public:
        using slot_type = Slot<Args...>;
        using callback_type = std::function<void(Args...)>;

    private:
        std::vector<slot_type*> slots;
        bool forgot = false; // true if slots contain any nullptr


        // This is a convenience accessor, it does not guarantee invariants.
        void remember(slot_type *slot) noexcept
        {
            slots.push_back(slot);
        }


        /*
         * This is a convenience accessor, it does not guarantee invariants.
         * Replace a given entry by nullptr, so the emit() loop doesn't break.
         */
        void forget(slot_type *slot) noexcept
        {
            auto i = std::find(slots.begin(), slots.end(), slot);
            if (i != slots.end()) {
                forgot = true;
                *i = nullptr;
            }
        }


        // Removes forgotten signals.
        void purge() noexcept
        {
            if (forgot) {
                slots.erase(std::remove(slots.begin(),
                                        slots.end(),
                                        nullptr),
                            slots.end());
                forgot = false;
            }
        }

    public:

        Signal() = default;


        ~Signal()
        {
            disconnectAll();
        }


        // Move constructor: makes every slot remember this one.
        Signal(Signal&& other) :
            slots {std::move(other.slots)},
            forgot{other.forgot}
        {
            other.slots.clear(); // make sure it forgets every slot
            for (slot_type *slot : slots)
                slot->remember(this);
        }


        // Move assignment: makes every slot remember this one.
        Signal& operator=(Signal&& other)
        {
            if (&other != this) {
                disconnectAll();
                slots = std::move(other.slots);
                other.slots.clear(); // make sure it forgets every slot
                forgot = other.forgot;
                for (slot_type *slot : slots)
                    slot->remember(this);
            }
            return *this;
        }


        void emit(Args... args)
        {
            purge(); // removes forgotten entries.
            for (const slot_type *slot : slots)
                slot->notify(args...);
        }


        // Sane as emit()
        void operator()(Args... args)
        {
            emit(args...);
        }


        // Convenient way to construct a Slot connected to this Signal.
        slot_type connect(const callback_type& callback)
        {
            return slot_type(*this, callback);
        }


        void disconnectAll()
        {
            for (slot_type *slot : slots)
                if (slot)
                    slot->forget();
            slots.clear();
        }


        int size() const
        {
            int size = 0;
            for (slot_type *slot : slots)
                if (slot)
                    size++;
            return size;
        }


        // Forbid copying.
        Signal(const Signal&) = delete;
        Signal& operator=(const Signal&) = delete;


        friend Slot<Args...>;
    };


    template<typename... Args>
    class Slot {
    public:
        using signal_type = Signal<Args...>;
        using callback_type = typename signal_type::callback_type;

    private:
        signal_type  *sig;
        callback_type callback;


        void remember(signal_type* s)
        {
            sig = s;
        }


        void forget()
        {
            sig = nullptr;
        }

    public:

        // Default constructor: not connected, no callback
        Slot() noexcept :
            sig{nullptr}
        {}


        // Normal constructor, connected to signal, and with callback
        Slot(signal_type& s, const callback_type& func) :
            sig{&s},
            callback{func}
        {
            sig->remember(this);
        }


        // Move constructor
        Slot(Slot&& other) :
            sig{other.sig},
            callback{std::move(other.callback)}
        {
            other.disconnect();
            sig->remember(this);
        }


        // Move assignment
        Slot& operator=(Slot&& other)
        {
            if (this == &other)
                return *this;

            signal_type *othersig = other.sig;

            disconnect();
            other.disconnect();

            sig = othersig;
            sig->remember(this);
            callback = std::move(other.callback);
            return *this;
        }


        ~Slot()
        {
            disconnect();
        }


        void disconnect()
        {
            if (sig)
                sig->forget(this);
            sig = nullptr;
        }


        bool connected() const
        {
            return sig != nullptr;
        }


        // called by Signal
        void notify(Args... args) const
        {
            callback(args...);
        }


        // forbid copying
        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;


        friend class Signal<Args...>;
    };

}


#endif
