#ifndef VG_GRAPH_SYNCHRONIZER_H
#define VG_GRAPH_SYNCHRONIZER_H

/**
 * \file graph_synchronizer.hpp: define a GraphSynchronizer that can manage
 * concurrent access and updates to a VG graph.
 */

#include "vg.hpp"
#include "path_index.hpp"

#include <thread>
#include <mutex>
#include <condition_variable>

namespace vg {

using namespace std;

/**
 * Let threads get exclusive locks on subgraphs of a vg graph, for reading and
 * editing. Whan a subgraph is locked, a copy is accessible through the lock
 * object and the underlying graph can be edited (through the lock) without
 * affecting any other locked subgraphs.
 *
 * A thread may only hold a lock on a single subgraph at a time. Trying to lock
 * another subgraph while you already have a subgraph locked is likely to result
 * in a deadlock.
 */ 
class GraphSynchronizer {

public:

    /**
     * Create a GraphSynchronizer for synchronizing on parts of the given graph.
     */
    GraphSynchronizer(VG& graph);
    
    /**
     * Since internally we keep PathIndexes for paths in the graph, we expose
     * this method for getting the strings for paths.
     */
    const string& get_path_sequence(const string& path_name);
    
    /**
     * This represents a request to lock a particular context on a particular
     * GraphSynchronizer. It fulfils the BasicLockable concept requirements, so
     * you can wait on it with std::unique_lock.
     */
    class Lock {
    public:
        
        /**
         * Create a request to lock a certain radius around a certain position
         * along a certain path in the graph controlled by the given
         * synchronizer.
         */
        Lock(GraphSynchronizer& synchronizer, const string& path_name, size_t path_offset, size_t context_bases, bool reflect);
        
        /**
         * Block until a lock is obtained.
         */
        void lock();
        
        /**
         * If a lock is held, unlock it.
         */
        void unlock();
        
        /**
         * May only be called when locked. Grab the subgraph that was extracted
         * when the lock was obtained. Does not contain any path information.
         */
        VG& get_subgraph();
        
        /**
         * May only be called when locked. Apply an edit against the base graph
         * and return the resulting translation. Note that this updates only the
         * underlying VG graph, not the copy of the locked subgraph stored in
         * the lock. Also note that the edit may only edit locked nodes.
         *
         * Edit operations will create new nodes, and cannot delete nodes or
         * apply changes (other than dividing and connecting) to existing nodes.
         *
         * Any new nodes created are created already locked.
         */
        vector<Translation> apply_edit(const Path& path);
        
    protected:
    
        /// This points back to the synchronizer we synchronize with when we get locked.
        GraphSynchronizer& synchronizer;
        
        // These hold the actual lock request we represent
        string path_name;
        size_t path_offset;
        size_t context_bases;
        bool reflect; // Should we bounce off node ends?
        
        /// This is the subgraph that got extracted during the locking procedure.
        VG subgraph;
        
        /// These are the nodes connected to the subgraph but not actually
        /// available for editing. We just need no one else to edit them.
        set<id_t> periphery;
        
        /// This is the set of nodes that this lock has currently locked.
        set<id_t> locked_nodes;
    };
    
protected:
    
    /// The graph we manage
    VG& graph;
    
    /// We use this to lock the whole graph, for when we're exploring and trying
    /// to lock a context, or for when we're making an edit, or for when we're
    /// trying to lock a context, or for when we're working with the
    /// PathIndexes. It's only ever held during functions in this class or
    /// internal classes (monitor-style), so we don't need it to be a recursive
    /// mutex.
    mutex whole_graph_lock;
    
    /// We have one condition variable where we have blocked all the threads
    /// that are waiting to lock subgraphs but couldn't the first time because
    /// we ran into already locked nodes. When nodes get unlocked, we wake them
    /// all up, and they each grab the mutex and check, one at a time, to see if
    /// they can have all their nodes this time.
    condition_variable wait_for_region;
    
    /// We need indexes of all the paths that someone might want to use as a
    /// basis for locking. This holds a PathIndex for each path we touch by path
    /// name.
    map<string, PathIndex> indexes;
    
    /**
     * Get the index for the given path name. Lock on the indexes and graph must
     * be held already.
     */
    PathIndex& get_path_index(const string& path_name);
    
    /**
     * Update all the path indexes according to the given translations. Lock on
     * the indexes and graph must be held already.
     */
    void update_path_indexes(const vector<Translation>& translations);
    
    /// This holds all the node IDs that are currently locked by someone
    set<id_t> locked_nodes;


};


}

#endif
