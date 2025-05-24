import threading
import msgpack  # Using MessagePack for compact binary serialization
import os

class EmbeddedKeyValueStore:
    def __init__(self, filename='store.dat', log_filename='log.dat', condition_key=None, condition_value=None):
        """
        Initializes the key-value store.

        :param filename: The file where the key-value store is persisted.
        :param log_filename: The file where logs of operations are stored.
        :param condition_key: The key to check against for edge-triggered persistence.
        :param condition_value: The value to check for the condition_key.
        """
        self.filename = filename
        self.log_filename = log_filename
        self.condition_key = condition_key
        self.condition_value = condition_value
        self.lock = threading.Lock()  # Synchronization lock for thread safety
        self.del_file()
        self.load_store()  # Load existing store data from file
        self.load_log()    # Load existing log data from file
    
    def del_file(self):
        """
        clean up file for every new run.

        Delete the files during init.
        """
        if os.path.exists(self.filename):
            os.remove(self.filename)
        
        if os.path.exists(self.log_filename):
            os.remove(self.log_filename)

    def load_store(self):
        """
        Loads the key-value store from a file.

        Data is deserialized from a binary format (MessagePack) to a Python dictionary.
        """
        if os.path.exists(self.filename):
            with open(self.filename, 'rb') as file:
                self.store = msgpack.unpack(file, raw=False)
        else:
            self.store = {}  # Initialize an empty store if file does not exist
    
    def save_store(self):
        """
        Saves the key-value store to a file.

        The store dictionary is serialized to a binary format (MessagePack) and written to the file.
        """
        with open(self.filename, 'wb') as file:
            msgpack.pack(self.store, file, use_bin_type=True)
    
    def load_log(self):
        """
        Loads the log of operations from a file.

        The log is deserialized from a binary format (MessagePack) to a list of dictionaries.
        """
        if os.path.exists(self.log_filename):
            with open(self.log_filename, 'rb') as file:
                self.log = msgpack.unpack(file, raw=False)
        else:
            self.log = []  # Initialize an empty log if file does not exist
    
    def append_to_log(self, operation, key, value):
        """
        Appends a record of a key-value operation to the log.

        This log records each operation for potential auditing or additional persistence actions.
        
        :param key: The key involved in the operation.
        :param value: The value involved in the operation.
        """
        self.log.append({'operation': operation, 'key': key, 'value': value})
        if self.check_condition(self.condition_key): # Check and handle the edge-triggered condition
            self.log.pop()

        with open(self.log_filename, 'wb') as file:
            msgpack.pack(self.log, file, use_bin_type=True)
    
    def put(self, key, value):
        """
        Stores a value associated with a key and persists it.

        The data is updated in the in-memory store and persisted to disk. Also, logs the operation.

        :param key: The key to store the value under.
        :param value: The value to store.
        """
        with self.lock:  # Ensure thread-safe access to the in-memory store
            self.store[key] = value
            self.save_store()  # Persist the updated store to disk
            self.append_to_log('put', key, value)  # Log the put operation
            #self.check_condition(key)  
    
    def get(self, key):
        """
        Retrieves a value associated with a key.

        Reads the value from the in-memory store and logs the access operation.

        :param key: The key whose value is to be retrieved.
        :return: The value associated with the key, or None if the key does not exist.
        """
        with self.lock:  # Ensure thread-safe access to the in-memory store
            value = self.store.get(key)
            self.append_to_log('get', key, value)  # Log the get operation
            return value
    
    def check_condition(self, key):
        """
        Checks if the specified condition is met and performs actions if true.

        The condition is checked after each put operation. If the condition is met, additional actions can be taken.

        :param key: The key to check against the condition.
        """
        if self.condition_key is not None and self.condition_value is not None:
            if key == self.condition_key and self.store.get(key) == self.condition_value:
                print(f"Condition met for key: {key}")
                return True
                # Additional actions could be added here, such as triggering other processes or notifications

# Example usage demonstrating the concurrency and condition checking
if __name__ == "__main__":
    store = EmbeddedKeyValueStore(condition_key='trigger', condition_value='activate')
    
    def writer():
        for i in range(8):
            store.put(f"key{i}", f"value{i}")
            if i == 5:
                store.put('trigger', 'activate')  # Trigger condition to test edge-triggered persistence
            print(f"{threading.current_thread().name}: Put key{i}: value{i}")
    
    def reader():
        for i in range(8):
            value = store.get(f"key{i}")
            print(f"{threading.current_thread().name}: Get key{i}: {value}")
    
    # Start writer and reader threads
    t1 = threading.Thread(target=writer, name="WRITE_THREAD_1")
    t2 = threading.Thread(target=reader, name="READ_THREAD_1")
    t3 = threading.Thread(target=reader, name="READ_THREAD_2")
    t4 = threading.Thread(target=reader, name="READ_THREAD_3")
    
    t1.start()
    t2.start()
    
    t1.join()
    t2.join()
    
    t3.start()
    t4.start()
    
    t3.join()
    t4.join()

    print(f"********* Reading back events recorded in the log **************")
    store.load_log()

    for record in store.log:
        print(f"{record}")
